/*
 * FILE: ui.h
 * MÔ TẢ: File header định nghĩa hàm khởi tạo và chạy giao diện người dùng (UI)
 */

#ifndef FM_UI_H
#define FM_UI_H

#include "fs.h"  // Include các hàm thao tác file system

/*
 * HÀM: fm_ui_run
 * MÔ TẢ: Khởi tạo giao diện ncurses và chạy vòng lặp chính của ứng dụng
 * THAM SỐ: startpath - Đường dẫn thư mục khởi đầu để hiển thị
 * TRẢ VỀ: 0 nếu thành công, giá trị khác 0 nếu lỗi
 */
int fm_ui_run(const char *startpath);

#endif // FM_UI_H
