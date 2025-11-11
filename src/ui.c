#define _XOPEN_SOURCE 700
#include "ui.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>

/* Format file size to human readable */
static void format_size(off_t size, char *buf, size_t bufsize) {
    if (size < 1024) snprintf(buf, bufsize, "%lldB", (long long)size);
    else if (size < 1024LL*1024LL) snprintf(buf, bufsize, "%.1fK", size/1024.0);
    else if (size < 1024LL*1024LL*1024LL) snprintf(buf, bufsize, "%.1fM", size/(1024.0*1024.0));
    else snprintf(buf, bufsize, "%.1fG", size/(1024.0*1024.0*1024.0));
}

/* Convert mode to permission string */
static void format_perms(mode_t mode, char *buf) {
    buf[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

/* Draw top header (single row) */
static void draw_header(WINDOW *win, const char *path, int count) {
    int w = getmaxx(win);
    werase(win);
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(win, 0, 0, " File Manager - Path: %s", path);
    mvwprintw(win, 0, w - 20, "Items: %d", count);
    wattroff(win, COLOR_PAIR(1) | A_BOLD);
    wnoutrefresh(win);
}

/* Draw file list into list window (row 0 reserved for column headers) */
static void draw_list(WINDOW *win, fm_entry *items, int count, int sel, int offset) {
    int h = getmaxy(win), w = getmaxx(win);
    werase(win);

    /* Column headers on row 0 */
    wattron(win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(win, 0, 0, " Perms      Size    Owner    Group   Modified          Name");
    wattroff(win, COLOR_PAIR(2) | A_BOLD);

    /* Items from row 1 .. h-1 */
    for (int row = 1; row < h && (row - 1 + offset) < count; ++row) {
        int idx = row - 1 + offset;
        fm_entry *e = &items[idx];

        char perms[12] = {0}, size_str[16] = {0}, owner[32] = {0}, group[32] = {0}, mtime[32] = {0};
        format_perms(e->st.st_mode, perms);
        format_size(e->st.st_size, size_str, sizeof(size_str));

        struct passwd *pw = getpwuid(e->st.st_uid);
        struct group *gr = getgrgid(e->st.st_gid);
        snprintf(owner, sizeof(owner), "%s", pw ? pw->pw_name : "?");
        snprintf(group, sizeof(group), "%s", gr ? gr->gr_name : "?");

        struct tm tm_buf;
        struct tm *tm = localtime_r(&e->st.st_mtime, &tm_buf);
        if (tm) strftime(mtime, sizeof(mtime), "%Y-%m-%d %H:%M", tm);
        else snprintf(mtime, sizeof(mtime), "0000-00-00 00:00");

        /* Determine color based on file type */
        int color_pair;
        if (S_ISDIR(e->st.st_mode)) {
            color_pair = 5;  /* Blue for directories */
        } else if (S_ISLNK(e->st.st_mode)) {
            color_pair = 7;  /* Cyan for symbolic links */
        } else {
            color_pair = 6;  /* Green for regular files */
        }

        /* Apply selection highlight or type color */
        if (idx == sel) {
            wattron(win, COLOR_PAIR(3) | A_REVERSE);
        } else {
            wattron(win, COLOR_PAIR(color_pair));
        }

        /* Print with safe width (truncate name if too long) */
        int name_col = 56;
        char name_fmt[64];
        snprintf(name_fmt, sizeof(name_fmt), " %%-%ds %%s", name_col - 1); /* name area reserved (not perfect but OK) */

        /* Better: print fixed fields then name separately to avoid printf width surprises */
        mvwprintw(win, row, 0, " %-10s %7s %-8s %-8s %s  ",
                  perms, size_str, owner, group, mtime);

        /* Print name; ensure it doesn't overflow window width */
        int x = getcurx(win);
        int avail = w - x - 1;
        if (avail > 0) {
            char truncated[PATH_MAX+1];
            if ((int)strlen(e->name) > avail) {
                strncpy(truncated, e->name, avail-3);
                truncated[avail-3] = '\0';
                strcat(truncated, "...");
                mvwprintw(win, row, x, "%s", truncated);
            } else {
                mvwprintw(win, row, x, "%s", e->name);
            }
        }

        /* Turn off attributes */
        if (idx == sel) {
            wattroff(win, COLOR_PAIR(3) | A_REVERSE);
        } else {
            wattroff(win, COLOR_PAIR(color_pair));
        }
    }

    wnoutrefresh(win);
}

/* Show one-line status message at bottom */
static void show_status(WINDOW *win, const char *msg) {
    werase(win);
    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, 0, 0, " %s", msg);
    wattroff(win, COLOR_PAIR(4));
    wnoutrefresh(win);
}

/* Help bar (bottom) */
static void draw_help_bar(WINDOW *win) {
    werase(win);
    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, 0, 0, " [q]Quit [Enter]Open [Bksp]Up [n]NewDir [f]NewFile [d]Del [r]Rename [c]Copy [i]Info [o]View");
    wattroff(win, COLOR_PAIR(4));
    wnoutrefresh(win);
}

/* Prompt input in provided window; result placed in buf */
static int prompt_input(WINDOW *win, const char *prompt, char *buf, int bufsize) {
    echo();
    curs_set(1);
    werase(win);
    mvwprintw(win, 0, 0, "%s", prompt);
    wclrtoeol(win);
    wrefresh(win);
    /* Use wgetnstr so it's shown in the given window */
    wgetnstr(win, buf, bufsize - 1);
    noecho();
    curs_set(0);
    return 0;
}

/* Resize windows when terminal changes size */
static void resize_windows(WINDOW **header, WINDOW **listw, WINDOW **status) {
    int h, w; getmaxyx(stdscr, h, w);

    /* Resize header (1 row) */
    wresize(*header, 1, w);
    mvwin(*header, 0, 0);

    /* Resize list window between header and status */
    int list_h = (h >= 3) ? (h - 2) : 1;
    wresize(*listw, list_h, w);
    mvwin(*listw, 1, 0);

    /* Resize status (1 row) at bottom */
    wresize(*status, 1, w);
    mvwin(*status, h - 1, 0);

    /* Make sure curses redraws everything */
    clear();
    refresh();
}

/* Main UI loop */
int fm_ui_run(const char *startpath) {
    if (!startpath) startpath = ".";

    initscr();
    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Terminal does not support colors\n");
        return 1;
    }
    start_color();
    use_default_colors();
    init_pair(1, COLOR_CYAN, -1);    /* header */
    init_pair(2, COLOR_YELLOW, -1);  /* column headers */
    init_pair(3, COLOR_WHITE, -1);   /* selection */
    init_pair(4, COLOR_BLACK, COLOR_WHITE); /* status/help bar */
    init_pair(5, COLOR_BLUE, -1);    /* directory */
    init_pair(6, COLOR_GREEN, -1);   /* regular file */
    init_pair(7, COLOR_CYAN, -1);    /* symbolic link */

    noecho();
    curs_set(0);

    /* IMPORTANT: keep stdscr leaveok = FALSE so initial draw causes proper cursor movement/scroll */
    keypad(stdscr, TRUE);
    scrollok(stdscr, FALSE);
    leaveok(stdscr, FALSE);

    int h, w; getmaxyx(stdscr, h, w);
    WINDOW *header = newwin(1, w, 0, 0);
    WINDOW *listw = newwin((h >= 3) ? (h - 2) : 1, w, 1, 0);
    WINDOW *status = newwin(1, w, h - 1, 0);

    /* For the child windows, also prefer normal cursor movements (leaveok FALSE) */
    scrollok(header, FALSE); leaveok(header, FALSE);
    scrollok(listw, FALSE); leaveok(listw, FALSE);
    scrollok(status, FALSE); leaveok(status, FALSE);

    /* Draw initial blank screen so user sees app immediately */
    clear();
    refresh();

    char cwd[PATH_MAX];
    if (realpath(startpath, cwd) == NULL) {
        /* fallback to given path (maybe relative) */
        strncpy(cwd, startpath, sizeof(cwd)-1);
        cwd[sizeof(cwd)-1] = '\0';
    }

    fm_entry *items = NULL;
    int count = 0;
    int sel = 0, offset = 0;

    while (1) {
        int r = fm_read_dir(cwd, &items);
        if (r < 0) {
            char errbuf[256];
            snprintf(errbuf, sizeof(errbuf), "Error reading directory: %s", strerror(errno));
            show_status(status, errbuf);
            wnoutrefresh(header);
            wnoutrefresh(listw);
            wnoutrefresh(status);
            doupdate();
            wgetch(stdscr);
            break;
        }
        count = r;
        if (count == 0) { sel = 0; offset = 0; }
        if (sel >= count) sel = count > 0 ? count - 1 : 0;
        if (sel < 0) sel = 0;
        if (offset < 0) offset = 0;

        /* Draw UI using wnoutrefresh then doupdate for flicker-free update */
        draw_header(header, cwd, count);
        draw_list(listw, items, count, sel, offset);
        draw_help_bar(status);
        doupdate();

        int ch = wgetch(stdscr);

        if (ch == 'q' || ch == 'Q') break;
        else if (ch == KEY_DOWN) {
            if (sel + 1 < count) sel++;
            /* if selection would fall off visible area, advance offset */
            int visible_rows = getmaxy(listw) - 1; /* row 0 is headers */
            if (sel - offset >= visible_rows) offset = sel - visible_rows + 1;
        }
        else if (ch == KEY_UP) {
            if (sel > 0) sel--;
            if (sel < offset) offset = sel;
        }
        else if (ch == 10 || ch == KEY_ENTER) {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            if (e->is_dir) {
                if (realpath(e->path, cwd) == NULL) {
                    strncpy(cwd, e->path, sizeof(cwd)-1);
                    cwd[sizeof(cwd)-1] = '\0';
                }
                sel = offset = 0;
            } else {
                char buf[512];
                char perms[12], size_str[16];
                format_perms(e->st.st_mode, perms);
                format_size(e->st.st_size, size_str, sizeof(size_str));
                struct passwd *pw = getpwuid(e->st.st_uid);
                struct group *gr = getgrgid(e->st.st_gid);
                struct tm tm_buf;
                struct tm *tm = localtime_r(&e->st.st_mtime, &tm_buf);
                snprintf(buf, sizeof(buf), "%s | %s | %s:%s | %04d-%02d-%02d %02d:%02d | Inode: %llu | Press any key...",
                         perms, size_str,
                         pw ? pw->pw_name : "?", gr ? gr->gr_name : "?",
                         tm ? (tm->tm_year+1900) : 0, tm ? (tm->tm_mon+1) : 0, tm ? tm->tm_mday : 0,
                         tm ? tm->tm_hour : 0, tm ? tm->tm_min : 0,
                         (unsigned long long)e->st.st_ino);
                show_status(status, buf);
                doupdate();
                wgetch(stdscr);
            }
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            /* go up one directory */
            char *p = strrchr(cwd, '/');
            if (p && p != cwd) *p = '\0';
            else strcpy(cwd, "/");
            sel = offset = 0;
        }
        else if (ch == 'n' || ch == 'N') {
            char name[PATH_MAX];
            prompt_input(status, "New directory name: ", name, sizeof(name));
            if (strlen(name) > 0) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s/%s", cwd, name);
                if (fm_mkdir(path) == 0) {
                    show_status(status, "✓ Directory created successfully. Press any key...");
                } else {
                    show_status(status, "✗ Failed to create directory. Press any key...");
                }
                doupdate();
                wgetch(stdscr);
            }
        }
        else if (ch == 'f' || ch == 'F') {
            char name[PATH_MAX];
            prompt_input(status, "New file name: ", name, sizeof(name));
            if (strlen(name) > 0) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s/%s", cwd, name);
                if (fm_create_file(path) == 0) {
                    show_status(status, "✓ File created successfully. Press any key...");
                } else {
                    show_status(status, "✗ Failed to create file (may already exist). Press any key...");
                }
                doupdate();
                wgetch(stdscr);
            }
        }
        else if (ch == 'd' || ch == 'D') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            char q[512]; snprintf(q, sizeof(q), "Delete '%s'? [y/n]", e->name);
            show_status(status, q);
            doupdate();
            int c = wgetch(stdscr);
            if (c == 'y' || c == 'Y') {
                if (fm_remove(e->path) == 0) {
                    show_status(status, "✓ Deleted successfully. Press any key...");
                    if (sel >= count - 1 && sel > 0) sel--;
                } else {
                    show_status(status, "✗ Delete failed (may be non-empty dir). Press any key...");
                }
                doupdate();
                wgetch(stdscr);
            }
        }
        else if (ch == 'r' || ch == 'R') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            char name[PATH_MAX];
            prompt_input(status, "Rename to: ", name, sizeof(name));
            if (strlen(name) > 0) {
                char path[PATH_MAX];
                snprintf(path, sizeof(path), "%s/%s", cwd, name);
                if (fm_rename(e->path, path) == 0) {
                    show_status(status, "✓ Renamed successfully. Press any key...");
                } else {
                    show_status(status, "✗ Rename failed. Press any key...");
                }
                doupdate();
                wgetch(stdscr);
            }
        }
        else if (ch == 'c' || ch == 'C') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            if (e->is_dir) {
                show_status(status, "✗ Copy directory not supported. Press any key...");
                doupdate();
                wgetch(stdscr);
            } else {
                char name[PATH_MAX];
                prompt_input(status, "Copy to (name): ", name, sizeof(name));
                if (strlen(name) > 0) {
                    char path[PATH_MAX];
                    snprintf(path, sizeof(path), "%s/%s", cwd, name);
                    if (fm_copy_file(e->path, path) == 0) {
                        show_status(status, "✓ File copied successfully. Press any key...");
                    } else {
                        show_status(status, "✗ Copy failed. Press any key...");
                    }
                    doupdate();
                    wgetch(stdscr);
                }
            }
        }
        else if (ch == 'i' || ch == 'I') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            
            /* Build detailed info string */
            char info[2048];
            char perms[12], size_str[16];
            format_perms(e->st.st_mode, perms);
            format_size(e->st.st_size, size_str, sizeof(size_str));
            
            struct passwd *pw = getpwuid(e->st.st_uid);
            struct group *gr = getgrgid(e->st.st_gid);
            struct tm *tm_mtime = localtime(&e->st.st_mtime);
            struct tm *tm_atime = localtime(&e->st.st_atime);
            
            char mtime_str[64], atime_str[64];
            strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", tm_mtime);
            strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M:%S", tm_atime);
            
            const char *type = e->is_dir ? "Directory" : 
                              S_ISLNK(e->st.st_mode) ? "Symbolic Link" : "File";
            
            snprintf(info, sizeof(info), 
                     "Name: %s | Type: %s | Size: %s | Perms: %s | Owner: %s | Group: %s | Inode: %llu | Modified: %s | Accessed: %s | Press any key...",
                     e->name, type, size_str, perms,
                     pw ? pw->pw_name : "?", gr ? gr->gr_name : "?",
                     (unsigned long long)e->st.st_ino, mtime_str, atime_str);
            
            show_status(status, info);
            doupdate();
            wgetch(stdscr);
        }
        else if (ch == 'o' || ch == 'O') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            if (e->is_dir) {
                show_status(status, "Cannot open directory. Press any key...");
                doupdate();
                wgetch(stdscr);
                continue;
            }
            /* open with pager in normal mode */
            def_prog_mode();
            endwin();
            const char *pager = getenv("PAGER"); if (!pager) pager = "less";
            char cmd[PATH_MAX * 2];
            snprintf(cmd, sizeof(cmd), "%s '%s'", pager, e->path); /* note: simplistic quoting */
            system(cmd);
            reset_prog_mode();
            /* Force complete redraw */
            clearok(stdscr, TRUE);
            clear();
            refresh();
            /* redraw next loop iteration will draw everything */
        }
        else if (ch == KEY_RESIZE) {
            /* Recreate/resize windows to match new terminal size */
            resize_windows(&header, &listw, &status);
            /* Ensure wnoutrefresh/doupdate following next draw */
        }

        /* keep offset in valid range */
        if (offset > count - 1) offset = count > 0 ? count - 1 : 0;
        if (offset < 0) offset = 0;

        free(items);
        items = NULL;
    }

    /* cleanup */
    if (items) free(items);
    delwin(header);
    delwin(listw);
    delwin(status);
    endwin();
    return 0;
}