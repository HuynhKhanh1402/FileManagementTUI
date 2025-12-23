/*
 * FILE: ui.c
 * MÔ TẢ: Triển khai giao diện người dùng (TUI) sử dụng thư viện ncurses
 */

#define _XOPEN_SOURCE 700  // Định nghĩa để sử dụng các tính năng POSIX mở rộng
#include "ui.h"            // Include định nghĩa hàm UI
#include <ncurses.h>       // Thư viện tạo giao diện text user interface
#include <stdlib.h>        // malloc, free, system
#include <string.h>        // strlen, strcmp, strcpy,...
#include <pwd.h>           // getpwuid - lấy thông tin user từ UID
#include <grp.h>           // getgrgid - lấy thông tin group từ GID
#include <time.h>          // localtime, strftime - xử lý thời gian
#include <sys/types.h>     // Các kiểu dữ liệu hệ thống
#include <sys/stat.h>      // Thông tin file/thư mục
#include <limits.h>        // PATH_MAX
#include <unistd.h>        // stat, realpath
#include <errno.h>         // errno - mã lỗi hệ thống

/*
 * HÀM: format_size
 * MÔ TẢ: Chuyển đổi kích thước file sang dạng dễ đọc (B, K, M, G)
 * THAM SỐ:
 *   - size: Kích thước file theo byte
 *   - buf: Buffer để lưu kết quả
 *   - bufsize: Kích thước buffer
 */
static void format_size(off_t size, char *buf, size_t bufsize) {
    if (size < 1024)  // Nhỏ hơn 1KB -> hiển thị dạng Byte
        snprintf(buf, bufsize, "%lldB", (long long)size);
    else if (size < 1024LL*1024LL)  // Nhỏ hơn 1MB -> hiển thị dạng KB
        snprintf(buf, bufsize, "%.1fK", size/1024.0);
    else if (size < 1024LL*1024LL*1024LL)  // Nhỏ hơn 1GB -> hiển thị dạng MB
        snprintf(buf, bufsize, "%.1fM", size/(1024.0*1024.0));
    else  // Lớn hơn hoặc bằng 1GB -> hiển thị dạng GB
        snprintf(buf, bufsize, "%.1fG", size/(1024.0*1024.0*1024.0));
}

/*
 * HÀM: get_file_type
 * MÔ TẢ: Lấy ký tự đại diện cho loại file (giống lệnh ls -l)
 * THAM SỐ: mode - Chế độ file từ struct stat
 * TRẢ VỀ: Ký tự đại diện loại file
 */
static char get_file_type(mode_t mode) {
    if (S_ISREG(mode)) return '-';   // File thông thường
    if (S_ISDIR(mode)) return 'd';   // Thư mục (directory)
    if (S_ISLNK(mode)) return 'l';   // Symbolic link
    if (S_ISCHR(mode)) return 'c';   // Character device
    if (S_ISBLK(mode)) return 'b';   // Block device
    if (S_ISFIFO(mode)) return 'p';  // Named pipe (FIFO)
    if (S_ISSOCK(mode)) return 's';  // Socket
    return '?';                       // Không xác định
}

/*
 * HÀM: format_perms
 * MÔ TẢ: Chuyển đổi mode thành chuỗi quyền (rwxrwxrwx)
 * THAM SỐ:
 *   - mode: Chế độ file từ struct stat
 *   - buf: Buffer để lưu chuỗi quyền (cần ít nhất 10 ký tự)
 */
static void format_perms(mode_t mode, char *buf) {
    // Quyền của owner (chủ sở hữu)
    buf[0] = (mode & S_IRUSR) ? 'r' : '-';  // Read
    buf[1] = (mode & S_IWUSR) ? 'w' : '-';  // Write
    buf[2] = (mode & S_IXUSR) ? 'x' : '-';  // Execute
    
    // Quyền của group (nhóm)
    buf[3] = (mode & S_IRGRP) ? 'r' : '-';  // Read
    buf[4] = (mode & S_IWGRP) ? 'w' : '-';  // Write
    buf[5] = (mode & S_IXGRP) ? 'x' : '-';  // Execute
    
    // Quyền của others (người khác)
    buf[6] = (mode & S_IROTH) ? 'r' : '-';  // Read
    buf[7] = (mode & S_IWOTH) ? 'w' : '-';  // Write
    buf[8] = (mode & S_IXOTH) ? 'x' : '-';  // Execute
    
    buf[9] = '\0';  // Kết thúc chuỗi
}

/*
 * HÀM: draw_header
 * MÔ TẢ: Vẽ thanh header phía trên (hiển thị đường dẫn hiện tại và số lượng item)
 * THAM SỐ:
 *   - win: Cửa sổ ncurses để vẽ
 *   - path: Đường dẫn thư mục hiện tại
 *   - count: Số lượng entry trong thư mục
 */
static void draw_header(WINDOW *win, const char *path, int count) {
    int w = getmaxx(win);  // Lấy chiều rộng của cửa sổ
    
    werase(win);  // Xóa toàn bộ nội dung cửa sổ
    
    // Bật thuộc tính màu và in đậm
    wattron(win, COLOR_PAIR(1) | A_BOLD);
    
    // In thông tin đường dẫn ở góc trái
    mvwprintw(win, 0, 0, " File Manager - Path: %s", path);
    
    // In số lượng item ở góc phải
    mvwprintw(win, 0, w - 20, "Items: %d", count);
    
    // Tắt thuộc tính màu và in đậm
    wattroff(win, COLOR_PAIR(1) | A_BOLD);
    
    // Đánh dấu cửa sổ cần refresh (sẽ được update bởi doupdate())
    wnoutrefresh(win);
}

/*
 * HÀM: draw_list
 * MÔ TẢ: Vẽ danh sách file/thư mục với các thông tin chi tiết
 * THAM SỐ:
 *   - win: Cửa sổ ncurses để vẽ
 *   - items: Mảng các entry cần hiển thị
 *   - count: Số lượng entry
 *   - sel: Chỉ số entry đang được chọn
 *   - offset: Chỉ số entry đầu tiên hiển thị (cho scroll)
 */
static void draw_list(WINDOW *win, fm_entry *items, int count, int sel, int offset) {
    int h = getmaxy(win);  // Lấy chiều cao cửa sổ
    int w = getmaxx(win);  // Lấy chiều rộng cửa sổ
    
    werase(win);  // Xóa toàn bộ nội dung cửa sổ

    /* Vẽ tiêu đề các cột ở hàng 0: Loại Links Quyền Kích_thước Chủ Nhóm Thời_gian Tên */
    wattron(win, COLOR_PAIR(2) | A_BOLD);
    mvwprintw(win, 0, 0, " %s %5s %-9s %7s %-12s %-10s %-16s %s",
              "T", "Links", "Perms", "Size", "Owner", "Group", "Modified", "Name");
    wattroff(win, COLOR_PAIR(2) | A_BOLD);

    /* Hiển thị các entry từ hàng 1 đến hàng h-1 */
    for (int row = 1; row < h && (row - 1 + offset) < count; ++row) {
        int idx = row - 1 + offset;  // Chỉ số thực của entry trong mảng
        fm_entry *e = &items[idx];   // Con trỏ tới entry hiện tại

        // Lấy ký tự đại diện loại file
        char file_type = get_file_type(e->st.st_mode);
        
        // Số lượng hard link
        nlink_t links = e->st.st_nlink;
        
        // Chuỗi tạm để lưu các thông tin đã format
        char perms[12] = {0};      // Chuỗi quyền (rwxrwxrwx)
        char size_str[16] = {0};   // Kích thước dạng human-readable
        char owner[32] = {0};      // Tên chủ sở hữu
        char group[32] = {0};      // Tên nhóm
        char mtime[32] = {0};      // Thời gian sửa đổi
        
        // Format các thông tin
        format_perms(e->st.st_mode, perms);
        format_size(e->st.st_size, size_str, sizeof(size_str));

        // Lấy thông tin owner từ UID
        struct passwd *pw = getpwuid(e->st.st_uid);
        // Lấy thông tin group từ GID
        struct group *gr = getgrgid(e->st.st_gid);
        
        // Lưu tên owner/group (hoặc "?" nếu không tìm thấy)
        snprintf(owner, sizeof(owner), "%s", pw ? pw->pw_name : "?");
        snprintf(group, sizeof(group), "%s", gr ? gr->gr_name : "?");

        // Chuyển đổi timestamp thành chuỗi thời gian
        struct tm tm_buf;
        struct tm *tm = localtime_r(&e->st.st_mtime, &tm_buf);
        if (tm) 
            strftime(mtime, sizeof(mtime), "%Y-%m-%d %H:%M", tm);
        else 
            snprintf(mtime, sizeof(mtime), "0000-00-00 00:00");

        /* Xác định màu sắc dựa trên loại file */
        int color_pair;
        if (S_ISDIR(e->st.st_mode)) {
            color_pair = 5;  // Màu xanh dương cho thư mục
        } else if (S_ISLNK(e->st.st_mode)) {
            color_pair = 7;  // Màu cyan cho symbolic link
        } else {
            color_pair = 6;  // Màu xanh lá cho file thông thường
        }

        /* Áp dụng highlight cho entry được chọn hoặc màu theo loại */
        if (idx == sel) {
            wattron(win, COLOR_PAIR(3) | A_REVERSE);  // Đảo màu cho entry được chọn
        } else {
            wattron(win, COLOR_PAIR(color_pair));
        }

        /* In các trường cố định: Loại(1) Links(5) Quyền(9) Kích_thước(7) Owner(12) Group(10) Thời_gian(16) */
        mvwprintw(win, row, 0, " %c %5lu %-9s %7s %-12s %-10s %-16s ",
                  file_type, (unsigned long)links, perms, size_str, owner, group, mtime);

        /* In tên file/thư mục; đảm bảo không vượt quá chiều rộng cửa sổ */
        int x = getcurx(win);  // Lấy vị trí con trỏ hiện tại
        int avail = w - x - 1; // Số ký tự còn lại có thể in
        if (avail > 0) {
            char truncated[PATH_MAX+1];
            // Nếu tên quá dài, cắt bớt và thêm "..."
            if ((int)strlen(e->name) > avail) {
                strncpy(truncated, e->name, avail-3);
                truncated[avail-3] = '\0';
                strcat(truncated, "...");
                mvwprintw(win, row, x, "%s", truncated);
            } else {
                mvwprintw(win, row, x, "%s", e->name);
            }
        }

        /* Tắt các thuộc tính màu */
        if (idx == sel) {
            wattroff(win, COLOR_PAIR(3) | A_REVERSE);
        } else {
            wattroff(win, COLOR_PAIR(color_pair));
        }
    }

    // Đánh dấu cửa sổ cần refresh
    wnoutrefresh(win);
}


/*
 * HÀM: show_status
 * MÔ TẢ: Hiển thị thông báo trạng thái ở thanh dưới cùng
 * THAM SỐ:
 *   - win: Cửa sổ ncurses để vẽ
 *   - msg: Thông báo cần hiển thị
 */
static void show_status(WINDOW *win, const char *msg) {
    werase(win);  // Xóa nội dung cũ
    wattron(win, COLOR_PAIR(4));  // Bật màu cho thanh trạng thái
    mvwprintw(win, 0, 0, " %s", msg);  // In thông báo
    wattroff(win, COLOR_PAIR(4));  // Tắt màu
    wnoutrefresh(win);  // Đánh dấu cần refresh
}

/*
 * HÀM: view_file_info
 * MÔ TẢ: Hiển thị thông tin chi tiết về file/thư mục ở chế độ toàn màn hình
 * THAM SỐ: e - Con trỏ tới entry cần xem thông tin
 */
/*
 * HÀM: view_file_info
 * MÔ TẢ: Hiển thị thông tin chi tiết về file/thư mục ở chế độ toàn màn hình
 * THAM SỐ: e - Con trỏ tới entry cần xem thông tin
 */
static void view_file_info(const fm_entry *e) {
    // Chuỗi tạm để lưu thông tin đã format
    char perms[12], size_str[16];
    
    // Format quyền và kích thước
    format_perms(e->st.st_mode, perms);
    format_size(e->st.st_size, size_str, sizeof(size_str));
    
    // Lấy thông tin owner và group
    struct passwd *pw = getpwuid(e->st.st_uid);
    struct group *gr = getgrgid(e->st.st_gid);
    
    // Chuyển đổi thời gian modification và access
    struct tm *tm_mtime = localtime(&e->st.st_mtime);
    struct tm *tm_atime = localtime(&e->st.st_atime);
    char mtime_str[64], atime_str[64];
    strftime(mtime_str, sizeof(mtime_str), "%Y-%m-%d %H:%M:%S", tm_mtime);
    strftime(atime_str, sizeof(atime_str), "%Y-%m-%d %H:%M:%S", tm_atime);
    
    // Xác định loại file
    const char *type = e->is_dir ? "Directory" : S_ISLNK(e->st.st_mode) ? "Symbolic Link" : "File";

    int h, w;
    getmaxyx(stdscr, h, w);  // Lấy kích thước màn hình
    
    clear();  // Xóa toàn bộ màn hình
    
    // Vẽ tiêu đề
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 0, " File Info: %s", e->name);
    attroff(COLOR_PAIR(1) | A_BOLD);

    // Hiển thị các thông tin chi tiết
    int row = 2;
    attron(COLOR_PAIR(2));
    mvprintw(row++, 2, "Type:       %s", type);              // Loại
    if (!e->is_dir) {
        mvprintw(row++, 2, "Size:       %s", size_str);      // Kích thước (chỉ với file)
    }
    mvprintw(row++, 2, "Perms:      %s", perms);             // Quyền
    mvprintw(row++, 2, "Owner:      %s", pw ? pw->pw_name : "?");  // Chủ sở hữu
    mvprintw(row++, 2, "Group:      %s", gr ? gr->gr_name : "?");  // Nhóm
    mvprintw(row++, 2, "Inode:      %llu", (unsigned long long)e->st.st_ino);  // Số inode
    mvprintw(row++, 2, "Modified:   %s", mtime_str);         // Thời gian sửa đổi
    mvprintw(row++, 2, "Accessed:   %s", atime_str);         // Thời gian truy cập
    mvprintw(row++, 2, "Path:       %s", e->path);           // Đường dẫn đầy đủ
    attroff(COLOR_PAIR(2));

    // Hiển thị hướng dẫn ở cuối màn hình
    attron(COLOR_PAIR(4));
    mvprintw(h - 2, 0, " Press any key to return...");
    // Đổ đầy khoảng trắng cho đến cuối dòng
    for (int x = getcurx(stdscr); x < w; x++) addch(' ');
    attroff(COLOR_PAIR(4));
    
    refresh();  // Cập nhật màn hình
    getch();    // Đợi người dùng nhấn phím
    
    clear();    // Xóa màn hình trước khi quay lại
    refresh();
}

/*
 * HÀM: draw_help_bar
 * MÔ TẢ: Vẽ thanh trợ giúp (hiển thị các phím tắt) ở cuối màn hình
 * THAM SỐ: win - Cửa sổ ncurses để vẽ
 */
static void draw_help_bar(WINDOW *win) {
    werase(win);  // Xóa nội dung cũ
    wattron(win, COLOR_PAIR(4));  // Bật màu cho thanh trợ giúp
    // Hiển thị danh sách các phím tắt
    mvwprintw(win, 0, 0, " [q]Quit [Enter]Open [Bksp]Up [n]NewDir [f]NewFile [d]Del [r]Rename [m]Move [c]Copy [i]Info [o]View [e]Edit");
    wattroff(win, COLOR_PAIR(4));  // Tắt màu
    wnoutrefresh(win);  // Đánh dấu cần refresh
}

/*
 * HÀM: trim_string
 * MÔ TẢ: Loại bỏ khoảng trắng ở đầu và cuối chuỗi
 * THAM SỐ: str - Chuỗi cần xử lý (sẽ bị thay đổi trực tiếp)
 */
static void trim_string(char *str) {
    if (!str || !*str) return;  // Nếu chuỗi NULL hoặc rỗng thì return
    
    /* Loại bỏ khoảng trắng ở đầu */
    char *start = str;
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
        start++;  // Dịch con trỏ qua các ký tự trắng
    }
    
    /* Loại bỏ khoảng trắng ở cuối */
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = '\0';  // Thay thế ký tự trắng bằng null
        end--;
    }
    
    /* Di chuyển chuỗi đã trim về đầu nếu cần */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);  // +1 để copy cả ký tự null
    }
}

/*
 * HÀM: prompt_input
 * MÔ TẢ: Hiển thị prompt và nhận input từ người dùng
 * THAM SỐ:
 *   - win: Cửa sổ ncurses để hiển thị prompt
 *   - prompt: Chuỗi prompt
 *   - buf: Buffer để lưu input
 *   - bufsize: Kích thước buffer
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
static int prompt_input(WINDOW *win, const char *prompt, char *buf, int bufsize) {
    if (!win || !prompt || !buf || bufsize <= 0) return -1;  // Kiểm tra tham số
    
    echo();       // Bật echo để hiển thị ký tự người dùng nhập
    curs_set(1);  // Hiển thị con trỏ
    werase(win);  // Xóa cửa sổ
    
    /* Hiển thị prompt */
    wattron(win, COLOR_PAIR(4));
    mvwprintw(win, 0, 0, " %s", prompt);
    wattroff(win, COLOR_PAIR(4));
    wclrtoeol(win);  // Xóa phần còn lại của dòng
    wrefresh(win);   // Cập nhật cửa sổ
    
    /* Nhận input từ người dùng */
    int result = wgetnstr(win, buf, bufsize - 1);  // -1 để dành chỗ cho null terminator
    buf[bufsize - 1] = '\0';  // Đảm bảo chuỗi kết thúc bằng null
    
    /* Loại bỏ khoảng trắng thừa */
    trim_string(buf);
    
    noecho();      // Tắt echo
    curs_set(0);   // Ẩn con trỏ
    
    return result;
}

/*
 * HÀM: build_path
 * MÔ TẢ: Xây dựng đường dẫn đầy đủ một cách an toàn (tránh buffer overflow)
 * THAM SỐ:
 *   - dest: Buffer đích để lưu đường dẫn
 *   - dest_size: Kích thước buffer đích
 *   - dir: Thư mục cha
 *   - name: Tên file/thư mục con
 * TRẢ VỀ: 0 nếu thành công, -1 nếu đường dẫn quá dài
 */
static int build_path(char *dest, size_t dest_size, const char *dir, const char *name) {
    if (!dest || !dir || !name) return -1;  // Kiểm tra tham số
    
    // Xây dựng đường dẫn dạng "dir/name"
    int ret = snprintf(dest, dest_size, "%s/%s", dir, name);
    
    // Kiểm tra xem có bị truncate không
    return (ret >= (int)dest_size) ? -1 : 0;
}

/*
 * HÀM: show_status_and_wait
 * MÔ TẢ: Hiển thị thông báo trạng thái và đợi người dùng nhấn phím
 * THAM SỐ:
 *   - win: Cửa sổ để hiển thị thông báo
 *   - msg: Thông báo cần hiển thị
 */
static void show_status_and_wait(WINDOW *win, const char *msg) {
    show_status(win, msg);  // Hiển thị thông báo
    doupdate();             // Cập nhật màn hình
    wgetch(stdscr);         // Đợi người dùng nhấn phím
}

/*
 * HÀM: resize_windows
 * MÔ TẢ: Thay đổi kích thước các cửa sổ khi terminal thay đổi kích thước
 * THAM SỐ:
 *   - header: Con trỏ tới cửa sổ header
 *   - listw: Con trỏ tới cửa sổ danh sách
 *   - status: Con trỏ tới cửa sổ trạng thái
 */
static void resize_windows(WINDOW **header, WINDOW **listw, WINDOW **status) {
    int h, w; 
    getmaxyx(stdscr, h, w);  // Lấy kích thước mới của terminal

    /* Resize cửa sổ header (1 hàng) */
    wresize(*header, 1, w);
    mvwin(*header, 0, 0);

    /* Resize cửa sổ danh sách (giữa header và status) */
    int list_h = (h >= 3) ? (h - 2) : 1;
    wresize(*listw, list_h, w);
    mvwin(*listw, 1, 0);

    /* Resize cửa sổ status (1 hàng) ở cuối */
    wresize(*status, 1, w);
    mvwin(*status, h - 1, 0);

    /* Đảm bảo ncurses vẽ lại mọi thứ */
    clear();
    refresh();
}

/*
 * HÀM: view_file_content
 * MÔ TẢ: Hiển thị nội dung file với khả năng cuộn (scroll)
 * THAM SỐ: filepath - Đường dẫn đến file cần xem
 */
static void view_file_content(const char *filepath) {
    char *content = NULL;
    // Đọc toàn bộ nội dung file
    ssize_t size = fm_read_file(filepath, &content);
    
    // Kiểm tra lỗi khi đọc file
    if (size < 0 || !content) {
        /* Hiển thị thông báo lỗi */
        clear();
        mvprintw(0, 0, "Error: Unable to read file '%s'", filepath);
        mvprintw(1, 0, "Press any key to return...");
        refresh();
        getch();
        return;
    }
    
    /* Tách nội dung thành các dòng */
    int line_count = 0;          // Số dòng hiện tại
    int line_cap = 1024;         // Dung lượng mảng dòng
    char **lines = malloc(line_cap * sizeof(char*));  // Mảng con trỏ tới các dòng
    if (!lines) {
        free(content);
        return;
    }
    
    // Duyệt qua nội dung và tách thành các dòng
    char *p = content;           // Con trỏ duyệt
    char *line_start = p;        // Điểm bắt đầu của dòng hiện tại
    while (*p) {
        if (*p == '\n') {  // Gặp ký tự xuống dòng
            /* Cấp phát bộ nhớ cho dòng và sao chép nội dung */
            int len = p - line_start;
            lines[line_count] = malloc(len + 1);
            if (lines[line_count]) {
                memcpy(lines[line_count], line_start, len);
                lines[line_count][len] = '\0';
                line_count++;
                
                /* Mở rộng mảng nếu cần */
                if (line_count >= line_cap) {
                    line_cap *= 2;
                    char **tmp = realloc(lines, line_cap * sizeof(char*));
                    if (tmp) lines = tmp;
                }
            }
            line_start = p + 1;  // Bắt đầu dòng mới
        }
        p++;
    }
    
    /* Xử lý dòng cuối cùng nếu không kết thúc bằng newline */
    if (line_start < p) {
        int len = p - line_start;
        lines[line_count] = malloc(len + 1);
        if (lines[line_count]) {
            memcpy(lines[line_count], line_start, len);
            lines[line_count][len] = '\0';
            line_count++;
        }
    }
    
    free(content);  // Giải phóng buffer nội dung gốc
    
    /* Hiển thị nội dung file với khả năng cuộn */
    int offset = 0;  // Chỉ số dòng đầu tiên hiển thị
    int h, w;
    
    while (1) {
        getmaxyx(stdscr, h, w);  // Lấy kích thước màn hình
        clear();
        
        /* Vẽ header */
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(0, 0, " File Viewer: %s", filepath);
        mvprintw(0, w - 30, " Lines: %d", line_count);
        attroff(COLOR_PAIR(1) | A_BOLD);
        
        /* Vùng hiển thị nội dung (dành 2 hàng cho header và footer) */
        int content_h = h - 2;
        for (int i = 0; i < content_h && (i + offset) < line_count; i++) {
            int line_idx = i + offset;
            
            /* Hiển thị số dòng và nội dung */
            attron(COLOR_PAIR(2));
            mvprintw(i + 1, 0, "%5d ", line_idx + 1);  // Số dòng (5 ký tự)
            attroff(COLOR_PAIR(2));
            
            /* Cắt bớt dòng nếu quá dài */
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
        
        /* Footer / Thanh trợ giúp */
        attron(COLOR_PAIR(4));
        mvprintw(h - 1, 0, " [q]Quit [UP/DOWN]Scroll [PgUp/PgDn]Page [Home]Top [End]Bottom");
        /* Đổ đầy khoảng trắng cho đến cuối dòng */
        for (int x = getcurx(stdscr); x < w; x++) addch(' ');
        attroff(COLOR_PAIR(4));
        
        refresh();
        
        // Xử lý phím nhấn
        int ch = getch();
        if (ch == 'q' || ch == 'Q' || ch == 27) break;  // q, Q hoặc ESC -> thoát
        else if (ch == KEY_DOWN) {  // Mũi tên xuống
            if (offset + content_h < line_count) offset++;
        }
        else if (ch == KEY_UP) {  // Mũi tên lên
            if (offset > 0) offset--;
        }
        else if (ch == KEY_NPAGE) {  // Page Down
            offset += content_h;
            if (offset + content_h > line_count) {
                offset = line_count - content_h;
                if (offset < 0) offset = 0;
            }
        }
        else if (ch == KEY_PPAGE) {  // Page Up
            offset -= content_h;
            if (offset < 0) offset = 0;
        }
        else if (ch == KEY_HOME) {  // Home -> về đầu file
            offset = 0;
        }
        else if (ch == KEY_END) {  // End -> về cuối file
            offset = line_count - content_h;
            if (offset < 0) offset = 0;
        }
    }
    
    /* Dọn dẹp bộ nhớ */
    for (int i = 0; i < line_count; i++) {
        free(lines[i]);
    }
    free(lines);
    
    /* Buộc vẽ lại toàn bộ khi quay về UI chính */
    clear();
    refresh();
}

/*
 * HÀM: fm_ui_run
 * MÔ TẢ: Hàm chính khởi tạo và chạy vòng lặp UI của File Manager
 * THAM SỐ: startpath - Đường dẫn thư mục khởi đầu
 * TRẢ VỀ: 0 nếu thành công, giá trị khác 0 nếu lỗi
 */
int fm_ui_run(const char *startpath) {
    if (!startpath) startpath = ".";  // Nếu không có startpath, dùng thư mục hiện tại

    // Khởi tạo ncurses
    initscr();
    
    // Kiểm tra terminal có hỗ trợ màu không
    if (!has_colors()) {
        endwin();  // Kết thúc ncurses
        fprintf(stderr, "Terminal does not support colors\n");
        return 1;
    }
    
    // Khởi tạo hệ thống màu
    start_color();
    use_default_colors();  // Sử dụng màu mặc định của terminal
    
    // Định nghĩa các cặp màu (foreground, background)
    init_pair(1, COLOR_CYAN, -1);              // Màu cho header
    init_pair(2, COLOR_YELLOW, -1);            // Màu cho tiêu đề cột
    init_pair(3, COLOR_WHITE, -1);             // Màu cho entry được chọn
    init_pair(4, COLOR_BLACK, COLOR_WHITE);    // Màu cho thanh trạng thái/trợ giúp
    init_pair(5, COLOR_BLUE, -1);              // Màu cho thư mục
    init_pair(6, COLOR_GREEN, -1);             // Màu cho file thông thường
    init_pair(7, COLOR_CYAN, -1);              // Màu cho symbolic link

    noecho();      // Không hiển thị ký tự người dùng nhập
    curs_set(0);   // Ẩn con trỏ

    /* LƯU Ý: Giữ stdscr leaveok = FALSE để vẽ đúng cursor movement/scroll */
    keypad(stdscr, TRUE);     // Bật chế độ nhận phím đặc biệt (arrow keys,...)
    scrollok(stdscr, FALSE);  // Tắt auto-scroll
    leaveok(stdscr, FALSE);   // Yêu cầu cập nhật vị trí con trỏ

    // Lấy kích thước màn hình
    int h, w; 
    getmaxyx(stdscr, h, w);
    
    // Tạo 3 cửa sổ con
    WINDOW *header = newwin(1, w, 0, 0);                           // Header (1 hàng)
    WINDOW *listw = newwin((h >= 3) ? (h - 2) : 1, w, 1, 0);      // Danh sách file (giữa)
    WINDOW *status = newwin(1, w, h - 1, 0);                       // Thanh trạng thái (1 hàng cuối)

    /* Thiết lập các cửa sổ con */
    scrollok(header, FALSE); leaveok(header, FALSE);
    scrollok(listw, FALSE); leaveok(listw, FALSE);
    scrollok(status, FALSE); leaveok(status, FALSE);

    /* Vẽ màn hình trống ban đầu để người dùng thấy app khởi động ngay */
    clear();
    refresh();

    // Buffer lưu đường dẫn thư mục hiện tại
    char cwd[PATH_MAX];
    
    // Chuyển đổi startpath thành đường dẫn tuyệt đối
    if (realpath(startpath, cwd) == NULL) {
        // Nếu realpath thất bại, dùng đường dẫn gốc (có thể là relative)
        strncpy(cwd, startpath, sizeof(cwd)-1);
        cwd[sizeof(cwd)-1] = '\0';
    }

    // Biến quản lý danh sách entry
    fm_entry *items = NULL;  // Mảng các entry
    int count = 0;           // Số lượng entry
    int sel = 0;             // Chỉ số entry đang được chọn
    int offset = 0;          // Chỉ số entry đầu tiên hiển thị (cho scroll)

    // Vòng lặp chính của UI
    // Vòng lặp chính của UI
    while (1) {
        // Đọc nội dung thư mục hiện tại
        int r = fm_read_dir(cwd, &items);
        if (r < 0) {  // Nếu có lỗi khi đọc thư mục
            char errbuf[256];
            snprintf(errbuf, sizeof(errbuf), "Error reading directory: %s", strerror(errno));
            show_status(status, errbuf);
            
            // Cập nhật màn hình
            wnoutrefresh(header);
            wnoutrefresh(listw);
            wnoutrefresh(status);
            doupdate();
            
            wgetch(stdscr);  // Đợi người dùng nhấn phím
            break;           // Thoát khỏi vòng lặp
        }
        
        count = r;  // Lưu số lượng entry
        
        // Điều chỉnh sel và offset nếu cần
        if (count == 0) { sel = 0; offset = 0; }          // Thư mục rỗng
        if (sel >= count) sel = count > 0 ? count - 1 : 0; // sel vượt quá số entry
        if (sel < 0) sel = 0;                             // sel âm
        if (offset < 0) offset = 0;                       // offset âm

        /* Vẽ UI - dùng wnoutrefresh rồi doupdate để tránh nhấp nháy */
        draw_header(header, cwd, count);
        draw_list(listw, items, count, sel, offset);
        draw_help_bar(status);
        doupdate();  // Cập nhật toàn bộ màn hình một lần

        // Đọc phím từ người dùng
        int ch = wgetch(stdscr);

        // Xử lý phím nhấn
        if (ch == 'q' || ch == 'Q') break;  // Phím 'q' hoặc 'Q' -> Thoát
        
        else if (ch == KEY_DOWN) {  // Mũi tên xuống -> Di chuyển selection xuống
            if (sel + 1 < count) sel++;
            
            /* Nếu selection rơi ra ngoài vùng hiển thị, tăng offset */
            int visible_rows = getmaxy(listw) - 1; // Hàng 0 là tiêu đề
            if (sel - offset >= visible_rows) offset = sel - visible_rows + 1;
        }
        
        else if (ch == KEY_UP) {  // Mũi tên lên -> Di chuyển selection lên
            if (sel > 0) sel--;
            if (sel < offset) offset = sel;  // Điều chỉnh offset nếu cần
        }
        
        else if (ch == 10 || ch == KEY_ENTER) {  // Phím Enter -> Mở file/thư mục
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
            int c = wgetch(stdscr);  // Đợi xác nhận
            if (c == 'y' || c == 'Y') {  // Nếu người dùng chọn 'y'
                if (fm_remove(e->path) == 0) {  // Thực hiện xóa
                    show_status_and_wait(status, "✓ Deleted successfully. Press any key...");
                    // Điều chỉnh selection nếu xóa entry cuối cùng
                    if (sel >= count - 1 && sel > 0) sel--;
                } else {
                    show_status_and_wait(status, "✗ Delete failed (may be non-empty dir). Press any key...");
                }
            }
        }
        
        else if (ch == 'r' || ch == 'R') {  // Phím 'r' -> Đổi tên
            if (count == 0) continue;  // Thư mục rỗng
            
            fm_entry *e = &items[sel];
            char name[PATH_MAX];
            
            // Nhận tên mới từ người dùng
            if (prompt_input(status, "Rename to:", name, sizeof(name)) == 0 && strlen(name) > 0) {
                char path[PATH_MAX * 2];
                if (build_path(path, sizeof(path), cwd, name) == -1) {
                    show_status_and_wait(status, "✗ Path too long. Press any key...");
                } else if (fm_rename(e->path, path) == 0) {  // Thực hiện đổi tên
                    show_status_and_wait(status, "✓ Renamed successfully. Press any key...");
                } else {
                    show_status_and_wait(status, "✗ Rename failed. Press any key...");
                }
            }
        }
        
        else if (ch == 'm' || ch == 'M') {  // Phím 'm' -> Di chuyển file/thư mục
            if (count == 0) continue;  // Thư mục rỗng
            
            fm_entry *e = &items[sel];
            char destdir[PATH_MAX];
            
            // Nhận đường dẫn đích từ người dùng
            if (prompt_input(status, "Move to directory (path):", destdir, sizeof(destdir)) != 0 || strlen(destdir) == 0) {
                continue;
            }
            
            char resolved[PATH_MAX * 2];    // Đường dẫn đích đã resolve
            char dest_path[PATH_MAX * 2];  // Đường dẫn đầy đủ bao gồm tên file
            
            /* Resolve đường dẫn thư mục đích */
            if (destdir[0] == '/') {  // Đường dẫn tuyệt đối
                strncpy(resolved, destdir, sizeof(resolved) - 1);
                resolved[sizeof(resolved) - 1] = '\0';
            } else {  // Đường dẫn tương đối
                if (build_path(resolved, sizeof(resolved), cwd, destdir) == -1) {
                    show_status_and_wait(status, "✗ Path too long. Press any key...");
                    continue;
                }
            }
            
            /* Kiểm tra xem thư mục đích có tồn tại không */
            struct stat st;
            if (stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) {
                show_status_and_wait(status, "✗ Destination directory does not exist. Press any key...");
                continue;
            }
            
            /* Xây dựng đường dẫn đích cuối cùng (bao gồm tên file) */
            if (build_path(dest_path, sizeof(dest_path), resolved, e->name) == -1) {
                show_status_and_wait(status, "✗ Destination path too long. Press any key...");
                continue;
            }
            
            // Thực hiện di chuyển
            if (fm_rename(e->path, dest_path) == 0) {
                show_status_and_wait(status, "✓ Moved successfully. Press any key...");
                if (sel > 0) sel--;  // Điều chỉnh selection
            } else {
                show_status_and_wait(status, "✗ Move failed (destination may already exist). Press any key...");
            }
        }
        
        else if (ch == 'c' || ch == 'C') {  // Phím 'c' -> Sao chép file
            if (count == 0) continue;  // Thư mục rỗng
            
            fm_entry *e = &items[sel];
            
            if (e->is_dir) {  // Không hỗ trợ sao chép thư mục
                show_status_and_wait(status, "✗ Copy directory not supported. Press any key...");
            } else {
                char name[PATH_MAX];
                
                // Nhận tên file đích từ người dùng
                if (prompt_input(status, "Copy to (name):", name, sizeof(name)) == 0 && strlen(name) > 0) {
                    char path[PATH_MAX * 2];
                    if (build_path(path, sizeof(path), cwd, name) == -1) {
                        show_status_and_wait(status, "✗ Path too long. Press any key...");
                    } else if (fm_copy_file(e->path, path) == 0) {  // Thực hiện sao chép
                        show_status_and_wait(status, "✓ File copied successfully. Press any key...");
                    } else {
                        show_status_and_wait(status, "✗ Copy failed. Press any key...");
                    }
                }
            }
        }
        
        else if (ch == 'i' || ch == 'I') {  // Phím 'i' -> Xem thông tin chi tiết
            if (count == 0) continue;  // Thư mục rỗng
            
            fm_entry *e = &items[sel];
            view_file_info(e);  // Hiển thị thông tin chi tiết
            
            /* Buộc vẽ lại toàn bộ màn hình */
            clearok(stdscr, TRUE);
            clear();
            refresh();
        }
        
        else if (ch == 'o' || ch == 'O') {  // Phím 'o' -> Xem nội dung file
            if (count == 0) continue;  // Thư mục rỗng
            
            fm_entry *e = &items[sel];
            
            if (e->is_dir) {  // Không thể mở thư mục
                show_status(status, "Cannot open directory. Press any key...");
                doupdate();
                wgetch(stdscr);
                continue;
            }
            
            /* Xem nội dung file với file viewer tùy chỉnh */
            view_file_content(e->path);
            
            /* Buộc vẽ lại toàn bộ màn hình */
            clearok(stdscr, TRUE);
            clear();
            refresh();
        }
        
        else if (ch == 'e' || ch == 'E') {  // Phím 'e' -> Chỉnh sửa file
            if (count == 0) continue;  // Thư mục rỗng
            
            fm_entry *e = &items[sel];
            
            if (e->is_dir) {  // Không thể chỉnh sửa thư mục
                show_status(status, "✗ Cannot edit directory. Press any key...");
                doupdate();
                wgetch(stdscr);
                continue;
            }
            
            /* Chỉnh sửa file với nano hoặc vim */
            def_prog_mode();   // Lưu trạng thái terminal hiện tại
            endwin();          // Tạm dừng ncurses
            
            int result = fm_edit_file(e->path);  // Mở editor
            
            reset_prog_mode(); // Khôi phục trạng thái terminal
            
            /* Buộc vẽ lại toàn bộ màn hình */
            clearok(stdscr, TRUE);
            clear();
            refresh();
            
            // Kiểm tra kết quả từ editor
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
            /* Danh sách file sẽ được refresh ở vòng lặp tiếp theo */
        }
        
        else if (ch == KEY_RESIZE) {  // Terminal thay đổi kích thước
            /* Tạo lại/resize các cửa sổ cho khớp với kích thước mới */
            resize_windows(&header, &listw, &status);
        }

        /* Giữ offset trong phạm vi hợp lệ */
        if (offset > count - 1) offset = count > 0 ? count - 1 : 0;
        if (offset < 0) offset = 0;

        // Giải phóng bộ nhớ của mảng items
        free(items);
        items = NULL;
    }

    /* Dọn dẹp khi thoát */
    if (items) free(items);  // Giải phóng mảng items nếu còn
    delwin(header);          // Xóa cửa sổ header
    delwin(listw);           // Xóa cửa sổ danh sách
    delwin(status);          // Xóa cửa sổ trạng thái
    endwin();                // Kết thúc chế độ ncurses
    
    return 0;  // Trả về 0 cho biết chương trình kết thúc thành công
}