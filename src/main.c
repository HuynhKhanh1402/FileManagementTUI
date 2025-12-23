/*
 * FILE: main.c
 * MÔ TẢ: Điểm bắt đầu của chương trình File Manager TUI
 */

#include "ui.h"      // Include hàm khởi tạo UI
#include <stdio.h>   // Include hàm I/O chuẩn như fprintf
#include <stdlib.h>  // Include các hàm tiện ích chuẩn

/*
 * HÀM: main
 * MÔ TẢ: Hàm chính của chương trình
 * THAM SỐ:
 *   - argc: Số lượng tham số dòng lệnh
 *   - argv: Mảng các chuỗi tham số dòng lệnh
 * TRẢ VỀ: 0 nếu chương trình chạy thành công, 1 nếu có lỗi
 */
int main(int argc, char **argv) {
    // Xác định thư mục khởi đầu: dùng argv[1] nếu có, không thì dùng thư mục hiện tại "."
    const char *start = argc > 1 ? argv[1] : ".";
    
    // Chạy giao diện người dùng
    if (fm_ui_run(start) != 0) {
        // Nếu có lỗi, in thông báo lỗi ra stderr
        fprintf(stderr, "Error running UI\n");
        return 1;  // Trả về mã lỗi 1
    }
    
    return 0;  // Trả về 0 cho biết chương trình chạy thành công
}
