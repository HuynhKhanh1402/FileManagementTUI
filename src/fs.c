/*
 * FILE: fs.c
 * MÔ TẢ: Triển khai các hàm thao tác với hệ thống file
 */

#define _XOPEN_SOURCE 700  // Định nghĩa để sử dụng các tính năng POSIX mở rộng
#include "fs.h"            // Include các định nghĩa hàm file system
#include <stdlib.h>        // malloc, realloc, free, system
#include <stdio.h>         // snprintf, perror
#include <dirent.h>        // opendir, readdir, closedir - đọc thư mục
#include <string.h>        // strcmp, memset
#include <strings.h>       // strcasecmp - so sánh chuỗi không phân biệt hoa thường
#include <errno.h>         // errno - mã lỗi hệ thống
#include <unistd.h>        // rmdir, unlink, read, write, close
#include <fcntl.h>         // open, O_RDONLY, O_WRONLY - mở file
#include <limits.h>        // PATH_MAX - độ dài tối đa của đường dẫn

/*
 * HÀM: entry_cmp
 * MÔ TẢ: Hàm so sánh 2 entry để sắp xếp (dùng cho qsort)
 * QUY TẮC SẮP XẾP:
 *   1. Thư mục luôn đứng trước file
 *   2. Trong cùng loại, sắp xếp theo tên (không phân biệt hoa thường)
 * THAM SỐ:
 *   - pa, pb: Con trỏ tới 2 entry cần so sánh
 * TRẢ VỀ: -1 nếu a < b, 0 nếu a == b, 1 nếu a > b
 */
static int entry_cmp(const void *pa, const void *pb) {
    const fm_entry *a = pa;  // Ép kiểu con trỏ void thành con trỏ fm_entry
    const fm_entry *b = pb;
    
    // Nếu a là thư mục mà b không phải -> a đứng trước
    if (a->is_dir && !b->is_dir) return -1;
    
    // Nếu a không phải thư mục mà b là thư mục -> a đứng sau
    if (!a->is_dir && b->is_dir) return 1;
    
    // Nếu cùng loại, so sánh theo tên (không phân biệt hoa thường)
    return strcasecmp(a->name, b->name);
}

/*
 * HÀM: fm_read_dir
 * MÔ TẢ: Đọc tất cả các entry trong thư mục và lưu vào mảng động
 * THAM SỐ:
 *   - path: Đường dẫn đến thư mục cần đọc
 *   - entries_out: Con trỏ tới con trỏ mảng fm_entry (sẽ được cấp phát động)
 * TRẢ VỀ: Số lượng entry đọc được, hoặc -1 nếu lỗi
 */
int fm_read_dir(const char *path, fm_entry **entries_out) {
    // Mở thư mục
    DIR *d = opendir(path);
    if (!d) return -1;  // Không mở được thư mục -> trả về lỗi
    
    struct dirent *ent;  // Cấu trúc chứa thông tin một entry từ readdir
    fm_entry *arr = NULL;  // Mảng động chứa các entry
    int cap = 0;  // Dung lượng hiện tại của mảng
    int n = 0;    // Số lượng entry đã đọc
    
    // Đọc từng entry trong thư mục
    while ((ent = readdir(d)) != NULL) {
        // Bỏ qua entry "." (thư mục hiện tại)
        if (strcmp(ent->d_name, ".") == 0) continue;
        
        // Kiểm tra xem mảng còn chỗ không, nếu không thì mở rộng
        if (n + 1 > cap) {
            // Nếu cap = 0 thì khởi tạo 64, không thì tăng gấp đôi
            cap = cap ? cap * 2 : 64;
            
            // Cấp phát lại bộ nhớ cho mảng
            fm_entry *tmp = realloc(arr, cap * sizeof(fm_entry));
            if (!tmp) {  // Nếu realloc thất bại
                free(arr);     // Giải phóng bộ nhớ cũ
                closedir(d);   // Đóng thư mục
                return -1;     // Trả về lỗi
            }
            arr = tmp;  // Cập nhật con trỏ mảng
        }
        
        // Lấy con trỏ tới entry hiện tại trong mảng
        fm_entry *e = &arr[n++];
        
        // Sao chép tên file/thư mục
        snprintf(e->name, sizeof(e->name), "%s", ent->d_name);
        
        // Xây dựng đường dẫn đầy đủ
        snprintf(e->path, sizeof(e->path), "%s/%s", path, ent->d_name);
        
        // Lấy thông tin chi tiết về file/thư mục (không follow symlink)
        if (lstat(e->path, &e->st) == -1) {
            // Nếu lstat thất bại, đặt giá trị mặc định
            memset(&e->st, 0, sizeof(e->st));  // Xóa sạch struct stat
            e->is_dir = 0;  // Coi như không phải thư mục
        } else {
            // Kiểm tra xem có phải là thư mục không
            e->is_dir = S_ISDIR(e->st.st_mode);
        }
    }
    
    closedir(d);  // Đóng thư mục
    
    // Sắp xếp mảng: thư mục trước, sau đó theo tên
    if (n > 0) qsort(arr, n, sizeof(fm_entry), entry_cmp);
    
    // Gán con trỏ mảng vào biến đầu ra
    *entries_out = arr;
    
    // Trả về số lượng entry đọc được
    return n;
}

/*
 * HÀM: fm_mkdir
 * MÔ TẢ: Tạo một thư mục mới với quyền 0755 (rwxr-xr-x)
 * THAM SỐ: path - Đường dẫn đến thư mục cần tạo
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_mkdir(const char *path) {
    // Gọi hàm mkdir của hệ thống với quyền 0755:
    // - Owner: đọc(4) + ghi(2) + thực thi(1) = 7
    // - Group: đọc(4) + thực thi(1) = 5
    // - Others: đọc(4) + thực thi(1) = 5
    return mkdir(path, 0755);
}

/*
 * HÀM: fm_remove
 * MÔ TẢ: Xóa file hoặc thư mục rỗng
 * THAM SỐ: path - Đường dẫn đến file/thư mục cần xóa
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_remove(const char *path) {
    struct stat st;  // Cấu trúc chứa thông tin file/thư mục
    
    // Lấy thông tin về file/thư mục (không follow symlink)
    if (lstat(path, &st) == -1) return -1;
    
    // Nếu là thư mục, dùng rmdir để xóa (chỉ xóa được thư mục rỗng)
    if (S_ISDIR(st.st_mode)) return rmdir(path);
    
    // Nếu là file, dùng unlink để xóa
    return unlink(path);
}

/*
 * HÀM: fm_rename
 * MÔ TẢ: Đổi tên hoặc di chuyển file/thư mục
 * THAM SỐ:
 *   - oldpath: Đường dẫn cũ
 *   - newpath: Đường dẫn mới
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_rename(const char *oldpath, const char *newpath) {
    // Gọi hàm rename của hệ thống
    return rename(oldpath, newpath);
}

/*
 * HÀM: fm_copy_file
 * MÔ TẢ: Sao chép nội dung file (không giữ metadata như quyền, thời gian)
 * THAM SỐ:
 *   - src: Đường dẫn file nguồn
 *   - dst: Đường dẫn file đích
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_copy_file(const char *src, const char *dst) {
    // Mở file nguồn ở chế độ chỉ đọc
    int in = open(src, O_RDONLY);
    if (in < 0) return -1;  // Không mở được file nguồn
    
    // Mở/tạo file đích ở chế độ ghi
    // O_WRONLY: chỉ ghi
    // O_CREAT: tạo file nếu chưa tồn tại
    // O_TRUNC: xóa nội dung cũ nếu file đã tồn tại
    // 0644: quyền rw-r--r--
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { 
        close(in);  // Đóng file nguồn nếu không tạo được file đích
        return -1; 
    }
    
    char buf[8192];  // Buffer 8KB để đọc/ghi dữ liệu
    ssize_t r;       // Số byte đọc được
    
    // Đọc từ file nguồn và ghi vào file đích
    while ((r = read(in, buf, sizeof(buf))) > 0) {
        char *p = buf;  // Con trỏ trỏ tới vị trí cần ghi
        ssize_t w;      // Số byte ghi được
        
        // Ghi hết dữ liệu đã đọc (vì write có thể không ghi hết một lần)
        while (r > 0 && (w = write(out, p, r)) > 0) { 
            r -= w;  // Giảm số byte còn lại cần ghi
            p += w;  // Dịch con trỏ tới vị trí tiếp theo
        }
        
        // Nếu write trả về lỗi
        if (w < 0) { 
            close(in); 
            close(out); 
            return -1; 
        }
    }
    
    // Đóng cả 2 file
    close(in); 
    close(out);
    
    return 0;  // Thành công
}

/*
 * HÀM: fm_create_file
 * MÔ TẢ: Tạo file rỗng mới (tương tự lệnh touch)
 * THAM SỐ: path - Đường dẫn đến file cần tạo
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi (ví dụ: file đã tồn tại)
 */
int fm_create_file(const char *path) {
    // Mở/tạo file với các flag:
    // O_WRONLY: chỉ ghi
    // O_CREAT: tạo file nếu chưa tồn tại
    // O_EXCL: thất bại nếu file đã tồn tại (đảm bảo tạo file mới)
    // 0644: quyền rw-r--r--
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0) return -1;  // Không tạo được file
    
    close(fd);  // Đóng file ngay sau khi tạo
    return 0;   // Thành công
}

/*
 * HÀM: fm_edit_file
 * MÔ TẢ: Mở file trong trình soạn thảo văn bản (ưu tiên nano, sau đó vim)
 * THAM SỐ: path - Đường dẫn đến file cần chỉnh sửa
 * TRẢ VỀ:
 *   - 0: Thành công
 *   - -1: File không tồn tại
 *   - -2: Không phải file thông thường
 *   - -3: Không tìm thấy trình soạn thảo (nano hoặc vim)
 *   - Giá trị khác: Mã lỗi từ trình soạn thảo
 */
int fm_edit_file(const char *path) {
    struct stat st;  // Cấu trúc chứa thông tin file
    
    // Kiểm tra xem file có tồn tại không
    if (stat(path, &st) != 0) {
        return -1;  // File không tồn tại
    }
    
    // Kiểm tra xem có phải là file thông thường không
    if (!S_ISREG(st.st_mode)) {
        return -2;  // Không phải file thông thường (có thể là thư mục, symlink,...)
    }
    
    // Biến lưu tên trình soạn thảo sẽ sử dụng
    const char *editor = NULL;
    
    // Kiểm tra xem nano có sẵn không
    // system() trả về 0 nếu lệnh thành công
    if (system("command -v nano > /dev/null 2>&1") == 0) {
        editor = "nano";  // Dùng nano
    }
    // Nếu không có nano, kiểm tra vim
    else if (system("command -v vim > /dev/null 2>&1") == 0) {
        editor = "vim";   // Dùng vim
    }
    // Không tìm thấy cả nano lẫn vim
    else {
        return -3;  // Không có trình soạn thảo phù hợp
    }
    
    // Xây dựng lệnh để chạy trình soạn thảo
    // Sử dụng dấu nháy đơn để bảo vệ đường dẫn có khoảng trắng
    char command[PATH_MAX * 2];
    snprintf(command, sizeof(command), "%s '%s'", editor, path);
    
    // Thực thi lệnh mở trình soạn thảo
    int result = system(command);
    
    return result;  // Trả về mã kết quả từ trình soạn thảo
}

/*
 * HÀM: fm_read_file
 * MÔ TẢ: Đọc toàn bộ nội dung file vào buffer
 * THAM SỐ:
 *   - path: Đường dẫn đến file cần đọc
 *   - content_out: Con trỏ tới con trỏ buffer (buffer sẽ được cấp phát bằng malloc)
 * TRẢ VỀ: Số byte đọc được nếu thành công, -1 nếu lỗi
 * LƯU Ý: Người gọi phải tự giải phóng bộ nhớ của *content_out
 */
ssize_t fm_read_file(const char *path, char **content_out) {
    // Mở file ở chế độ chỉ đọc
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;  // Không mở được file
    
    struct stat st;  // Cấu trúc chứa thông tin file
    
    // Lấy thông tin file để biết kích thước
    if (fstat(fd, &st) < 0) {
        close(fd);  // Đóng file trước khi trả về lỗi
        return -1;
    }
    
    // Lấy kích thước file
    size_t size = st.st_size;
    
    // Cấp phát buffer với kích thước = kích thước file + 1 byte cho ký tự null
    char *buf = malloc(size + 1);
    if (!buf) {
        close(fd);  // Không đủ bộ nhớ, đóng file
        return -1;
    }
    
    // Đọc nội dung file vào buffer
    ssize_t total = 0;  // Tổng số byte đã đọc
    while (total < (ssize_t)size) {
        // Đọc phần còn lại của file
        ssize_t n = read(fd, buf + total, size - total);
        
        if (n <= 0) {
            // Nếu read trả về 0 hoặc âm
            if (n < 0 && errno == EINTR) continue;  // Nếu bị gián đoạn, thử lại
            
            // Lỗi khác, giải phóng bộ nhớ và trả về lỗi
            free(buf);
            close(fd);
            return -1;
        }
        
        total += n;  // Cộng dồn số byte đã đọc
    }
    
    // Thêm ký tự null kết thúc chuỗi
    buf[total] = '\0';
    
    close(fd);  // Đóng file
    
    *content_out = buf;  // Gán con trỏ buffer cho biến đầu ra
    
    return total;  // Trả về số byte đã đọc
}
