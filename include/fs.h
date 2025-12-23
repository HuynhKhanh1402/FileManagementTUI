/* 
 * FILE: fs.h
 * MÔ TẢ: File header định nghĩa các cấu trúc dữ liệu và hàm thao tác với hệ thống file
 */

#ifndef FM_FS_H
#define FM_FS_H

#include <sys/types.h>  // Định nghĩa các kiểu dữ liệu hệ thống như ssize_t
#include <sys/stat.h>   // Định nghĩa struct stat để lấy thông tin file

/* 
 * STRUCT: fm_entry
 * MÔ TẢ: Cấu trúc lưu trữ thông tin của một entry (file hoặc thư mục)
 */
typedef struct fm_entry {
    char name[512];      // Tên của file/thư mục (không bao gồm đường dẫn)
    char path[1024];     // Đường dẫn đầy đủ đến file/thư mục
    struct stat st;      // Thông tin chi tiết về file (kích thước, quyền, thời gian,...)
    int is_dir;          // Cờ đánh dấu: 1 nếu là thư mục, 0 nếu là file
} fm_entry;

/*
 * HÀM: fm_read_dir
 * MÔ TẢ: Đọc tất cả các entry trong thư mục và lưu vào mảng động
 * THAM SỐ:
 *   - path: Đường dẫn đến thư mục cần đọc
 *   - entries_out: Con trỏ tới con trỏ mảng fm_entry (mảng sẽ được cấp phát bằng malloc)
 * TRẢ VỀ: Số lượng entry đọc được, hoặc -1 nếu lỗi
 * LƯU Ý: Người gọi phải tự giải phóng bộ nhớ của mảng entries_out
 */
int fm_read_dir(const char *path, fm_entry **entries_out);

/*
 * HÀM: fm_mkdir
 * MÔ TẢ: Tạo một thư mục mới
 * THAM SỐ: path - Đường dẫn đến thư mục cần tạo
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_mkdir(const char *path);

/*
 * HÀM: fm_remove
 * MÔ TẢ: Xóa file hoặc thư mục rỗng
 * THAM SỐ: path - Đường dẫn đến file/thư mục cần xóa
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_remove(const char *path);

/*
 * HÀM: fm_rename
 * MÔ TẢ: Đổi tên hoặc di chuyển file/thư mục
 * THAM SỐ:
 *   - oldpath: Đường dẫn cũ
 *   - newpath: Đường dẫn mới
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_rename(const char *oldpath, const char *newpath);

/*
 * HÀM: fm_copy_file
 * MÔ TẢ: Sao chép file (không giữ metadata như quyền, thời gian)
 * THAM SỐ:
 *   - src: Đường dẫn file nguồn
 *   - dst: Đường dẫn file đích
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_copy_file(const char *src, const char *dst);

/*
 * HÀM: fm_create_file
 * MÔ TẢ: Tạo file rỗng mới (tương tự lệnh touch)
 * THAM SỐ: path - Đường dẫn đến file cần tạo
 * TRẢ VỀ: 0 nếu thành công, -1 nếu lỗi
 */
int fm_create_file(const char *path);

/*
 * HÀM: fm_edit_file
 * MÔ TẢ: Mở file trong trình soạn thảo (nano hoặc vim)
 * THAM SỐ: path - Đường dẫn đến file cần chỉnh sửa
 * TRẢ VỀ: 0 nếu thành công, mã lỗi âm nếu thất bại
 */
int fm_edit_file(const char *path);

/*
 * HÀM: fm_read_file
 * MÔ TẢ: Đọc toàn bộ nội dung file vào buffer
 * THAM SỐ:
 *   - path: Đường dẫn đến file cần đọc
 *   - content_out: Con trỏ tới con trỏ buffer (buffer sẽ được cấp phát bằng malloc)
 * TRẢ VỀ: Số byte đọc được nếu thành công, -1 nếu lỗi
 * LƯU Ý: Người gọi phải tự giải phóng bộ nhớ của *content_out
 */
ssize_t fm_read_file(const char *path, char **content_out);

#endif // FM_FS_H
