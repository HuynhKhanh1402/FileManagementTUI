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

/* Get file type character */
static char get_file_type(mode_t mode) {
    if (S_ISREG(mode)) return '-';       /* Regular file */
    if (S_ISDIR(mode)) return 'd';       /* Directory */
    if (S_ISLNK(mode)) return 'l';       /* Symbolic link */
    if (S_ISCHR(mode)) return 'c';       /* Character device */
    if (S_ISBLK(mode)) return 'b';       /* Block device */
    if (S_ISFIFO(mode)) return 'p';      /* Named pipe (FIFO) */
    if (S_ISSOCK(mode)) return 's';      /* Socket */
    return '?';                           /* Unknown */
}

/* Convert mode to permission string */
static void format_perms(mode_t mode, char *buf) {
    buf[0] = (mode & S_IRUSR) ? 'r' : '-';
    buf[1] = (mode & S_IWUSR) ? 'w' : '-';
    buf[2] = (mode & S_IXUSR) ? 'x' : '-';
    buf[3] = (mode & S_IRGRP) ? 'r' : '-';
    buf[4] = (mode & S_IWGRP) ? 'w' : '-';
    buf[5] = (mode & S_IXGRP) ? 'x' : '-';
    buf[6] = (mode & S_IROTH) ? 'r' : '-';
    buf[7] = (mode & S_IWOTH) ? 'w' : '-';
    buf[8] = (mode & S_IXOTH) ? 'x' : '-';
    buf[9] = '\0';
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

    /* Column headers on row 0 - Type Links Perms Size Owner Group Modified Name */
    wattron(win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(win, 0, 0, " %s %5s %-9s %7s %-12s %-10s %-16s %s",
              "T", "Links", "Perms", "Size", "Owner", "Group", "Modified", "Name");
    wattroff(win, COLOR_PAIR(2) | A_BOLD);

    /* Items from row 1 .. h-1 */
    for (int row = 1; row < h && (row - 1 + offset) < count; ++row) {
        int idx = row - 1 + offset;
        fm_entry *e = &items[idx];

        char file_type = get_file_type(e->st.st_mode);
        nlink_t links = e->st.st_nlink;
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

        /* Print fixed fields: Type(1) Links(5) Perms(9) Size(7) Owner(12) Group(10) Modified(16) */
        mvwprintw(win, row, 0, " %c %5lu %-9s %7s %-12s %-10s %-16s ",
                  file_type, (unsigned long)links, perms, size_str, owner, group, mtime);

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
    mvwprintw(win, 0, 0, " [q]Quit [Enter]Open [Bksp]Up [n]NewDir [f]NewFile [d]Del [r]Rename [m]Move [c]Copy [i]Info [o]View [e]Edit");
    wattroff(win, COLOR_PAIR(4));
    wnoutrefresh(win);
}

/* Trim leading and trailing whitespace from string */
static void trim_string(char *str) {
    if (!str || !*str) return;
    
    /* Trim leading whitespace */
    char *start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;
    }
    
    /* Trim trailing whitespace */
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';
        end--;
    }
    
    /* Move trimmed string to beginning if needed */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/* Prompt input in provided window; result placed in buf (trimmed) */
static int prompt_input(WINDOW *win, const char *prompt, char *buf, int bufsize) {
    if (!win || !prompt || !buf || bufsize <= 0) return -1;
    
    echo();
    curs_set(1);
    werase(win);
    
    /* Display prompt */
    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, 0, 0, " %s", prompt);
    wattroff(win, COLOR_PAIR(4));
    wclrtoeol(win);
    wrefresh(win);
    
    /* Get input */
    int result = wgetnstr(win, buf, bufsize - 1);
    buf[bufsize - 1] = '\0';
    
    /* Trim whitespace */
    trim_string(buf);
    
    noecho();
    curs_set(0);
    
    return result;
}

/* Build path safely, return 0 on success, -1 on truncation */
static int build_path(char *dest, size_t dest_size, const char *dir, const char *name) {
    if (!dest || !dir || !name) return -1;
    
    int ret = snprintf(dest, dest_size, "%s/%s", dir, name);
    return (ret >= (int)dest_size) ? -1 : 0;
}

/* Show status message and wait for key press */
static void show_status_and_wait(WINDOW *win, const char *msg) {
    show_status(win, msg);
    doupdate();
    wgetch(stdscr);
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

/* View file content with scrolling capability */
static void view_file_content(const char *filepath) {
    char *content = NULL;
    ssize_t size = fm_read_file(filepath, &content);
    
    if (size < 0 || !content) {
        /* Show error message */
        clear();
        mvprintw(0, 0, "Error: Unable to read file '%s'", filepath);
        mvprintw(1, 0, "Press any key to return...");
        refresh();
        getch();
        return;
    }
    
    /* Split content into lines */
    int line_count = 0;
    int line_cap = 1024;
    char **lines = malloc(line_cap * sizeof(char*));
    if (!lines) {
        free(content);
        return;
    }
    
    char *p = content;
    char *line_start = p;
    while (*p) {
        if (*p == '\n') {
            /* Allocate space for line and copy it */
            int len = p - line_start;
            lines[line_count] = malloc(len + 1);
            if (lines[line_count]) {
                memcpy(lines[line_count], line_start, len);
                lines[line_count][len] = '\0';
                line_count++;
                
                /* Expand array if needed */
                if (line_count >= line_cap) {
                    line_cap *= 2;
                    char **tmp = realloc(lines, line_cap * sizeof(char*));
                    if (tmp) lines = tmp;
                }
            }
            line_start = p + 1;
        }
        p++;
    }
    
    /* Handle last line if it doesn't end with newline */
    if (line_start < p) {
        int len = p - line_start;
        lines[line_count] = malloc(len + 1);
        if (lines[line_count]) {
            memcpy(lines[line_count], line_start, len);
            lines[line_count][len] = '\0';
            line_count++;
        }
    }
    
    free(content);
    
    /* Display file content with scrolling */
    int offset = 0;
    int h, w;
    
    while (1) {
        getmaxyx(stdscr, h, w);
        clear();
        
        /* Header */
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 0, " File Viewer: %s", filepath);
        mvprintw(0, w - 30, " Lines: %d", line_count);
        attroff(COLOR_PAIR(1) | A_BOLD);
        
        /* Content area (leave 2 rows for header and footer) */
        int content_h = h - 2;
        for (int i = 0; i < content_h && (i + offset) < line_count; i++) {
            int line_idx = i + offset;
            /* Show line number and content */
            attron(COLOR_PAIR(2));
            mvprintw(i + 1, 0, "%5d ", line_idx + 1);
            attroff(COLOR_PAIR(2));
            
            /* Truncate line if too long */
            if (strlen(lines[line_idx]) > (size_t)(w - 7)) {
                char truncated[4096];
                strncpy(truncated, lines[line_idx], w - 10);
                truncated[w - 10] = '\0';
                strcat(truncated, "...");
                printw("%s", truncated);
            } else {
                printw("%s", lines[line_idx]);
            }
        }
        
        /* Footer / Help bar */
        attron(COLOR_PAIR(4));
        mvprintw(h - 1, 0, " [q]Quit [UP/DOWN]Scroll [PgUp/PgDn]Page [Home]Top [End]Bottom");
        /* Pad rest of line */
        for (int x = getcurx(stdscr); x < w; x++) addch(' ');
        attroff(COLOR_PAIR(4));
        
        refresh();
        
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) break; /* ESC also exits */
        else if (ch == KEY_DOWN) {
            if (offset + content_h < line_count) offset++;
        }
        else if (ch == KEY_UP) {
            if (offset > 0) offset--;
        }
        else if (ch == KEY_NPAGE) { /* Page Down */
            offset += content_h;
            if (offset + content_h > line_count) {
                offset = line_count - content_h;
                if (offset < 0) offset = 0;
            }
        }
        else if (ch == KEY_PPAGE) { /* Page Up */
            offset -= content_h;
            if (offset < 0) offset = 0;
        }
        else if (ch == KEY_HOME) {
            offset = 0;
        }
        else if (ch == KEY_END) {
            offset = line_count - content_h;
            if (offset < 0) offset = 0;
        }
    }
    
    /* Cleanup */
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);
    
    /* Force complete redraw when returning to main UI */
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
            if (prompt_input(status, "New directory name:", name, sizeof(name)) == 0 && strlen(name) > 0) {
                char path[PATH_MAX * 2];
                if (build_path(path, sizeof(path), cwd, name) == -1) {
                    show_status_and_wait(status, "✗ Path too long. Press any key...");
                } else if (fm_mkdir(path) == 0) {
                    show_status_and_wait(status, "✓ Directory created successfully. Press any key...");
                } else {
                    show_status_and_wait(status, "✗ Failed to create directory. Press any key...");
                }
            }
        }
        else if (ch == 'f' || ch == 'F') {
            char name[PATH_MAX];
            if (prompt_input(status, "New file name:", name, sizeof(name)) == 0 && strlen(name) > 0) {
                char path[PATH_MAX * 2];
                if (build_path(path, sizeof(path), cwd, name) == -1) {
                    show_status_and_wait(status, "✗ Path too long. Press any key...");
                } else if (fm_create_file(path) == 0) {
                    show_status_and_wait(status, "✓ File created successfully. Press any key...");
                } else {
                    show_status_and_wait(status, "✗ Failed to create file (may already exist). Press any key...");
                }
            }
        }
        else if (ch == 'd' || ch == 'D') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            char q[1024];
            snprintf(q, sizeof(q), "Delete '%s'? [y/n]", e->name);
            show_status(status, q);
            doupdate();
            int c = wgetch(stdscr);
            if (c == 'y' || c == 'Y') {
                if (fm_remove(e->path) == 0) {
                    show_status_and_wait(status, "✓ Deleted successfully. Press any key...");
                    if (sel >= count - 1 && sel > 0) sel--;
                } else {
                    show_status_and_wait(status, "✗ Delete failed (may be non-empty dir). Press any key...");
                }
            }
        }
        else if (ch == 'r' || ch == 'R') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            char name[PATH_MAX];
            if (prompt_input(status, "Rename to:", name, sizeof(name)) == 0 && strlen(name) > 0) {
                char path[PATH_MAX * 2];
                if (build_path(path, sizeof(path), cwd, name) == -1) {
                    show_status_and_wait(status, "✗ Path too long. Press any key...");
                } else if (fm_rename(e->path, path) == 0) {
                    show_status_and_wait(status, "✓ Renamed successfully. Press any key...");
                } else {
                    show_status_and_wait(status, "✗ Rename failed. Press any key...");
                }
            }
        }
        else if (ch == 'm' || ch == 'M') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            char destdir[PATH_MAX];
            if (prompt_input(status, "Move to directory (path):", destdir, sizeof(destdir)) != 0 || strlen(destdir) == 0) {
                continue;
            }
            
            char resolved[PATH_MAX * 2];
            char dest_path[PATH_MAX * 2];
            
            /* Resolve destination directory path */
            if (destdir[0] == '/') {
                /* Absolute path */
                strncpy(resolved, destdir, sizeof(resolved) - 1);
                resolved[sizeof(resolved) - 1] = '\0';
            } else {
                /* Relative path */
                if (build_path(resolved, sizeof(resolved), cwd, destdir) == -1) {
                    show_status_and_wait(status, "✗ Path too long. Press any key...");
                    continue;
                }
            }
            
            /* Check if destination directory exists */
            struct stat st;
            if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) {
                show_status_and_wait(status, "✗ Destination directory does not exist. Press any key...");
                continue;
            }
            
            /* Build final destination path with filename */
            if (build_path(dest_path, sizeof(dest_path), resolved, e->name) == -1) {
                show_status_and_wait(status, "✗ Destination path too long. Press any key...");
                continue;
            }
            
            if (fm_rename(e->path, dest_path) == 0) {
                show_status_and_wait(status, "✓ Moved successfully. Press any key...");
                if (sel > 0) sel--;
            } else {
                show_status_and_wait(status, "✗ Move failed (destination may already exist). Press any key...");
            }
        }
        else if (ch == 'c' || ch == 'C') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            if (e->is_dir) {
                show_status_and_wait(status, "✗ Copy directory not supported. Press any key...");
            } else {
                char name[PATH_MAX];
                if (prompt_input(status, "Copy to (name):", name, sizeof(name)) == 0 && strlen(name) > 0) {
                    char path[PATH_MAX * 2];
                    if (build_path(path, sizeof(path), cwd, name) == -1) {
                        show_status_and_wait(status, "✗ Path too long. Press any key...");
                    } else if (fm_copy_file(e->path, path) == 0) {
                        show_status_and_wait(status, "✓ File copied successfully. Press any key...");
                    } else {
                        show_status_and_wait(status, "✗ Copy failed. Press any key...");
                    }
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
            /* View file with custom file viewer */
            view_file_content(e->path);
            /* Force complete redraw */
            clearok(stdscr, TRUE);
            clear();
            refresh();
            /* redraw next loop iteration will draw everything */
        }
        else if (ch == 'e' || ch == 'E') {
            if (count == 0) continue;
            fm_entry *e = &items[sel];
            if (e->is_dir) {
                show_status(status, "✗ Cannot edit directory. Press any key...");
                doupdate();
                wgetch(stdscr);
                continue;
            }
            /* Edit file with nano or vim */
            def_prog_mode();
            endwin();
            int result = fm_edit_file(e->path);
            reset_prog_mode();
            /* Force complete redraw */
            clearok(stdscr, TRUE);
            clear();
            refresh();
            
            if (result == -2) {
                show_status(status, "✗ Can only edit regular files. Press any key...");
                doupdate();
                wgetch(stdscr);
            } else if (result == -3) {
                show_status(status, "✗ No editor found (nano or vim required). Press any key...");
                doupdate();
                wgetch(stdscr);
            } else if (result != 0) {
                show_status(status, "✗ Editor returned an error. Press any key...");
                doupdate();
                wgetch(stdscr);
            }
            /* File list will be refreshed in next loop iteration */
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