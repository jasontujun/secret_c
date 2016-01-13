//
// Created by jason on 2016/1/7.
//

#include <stddef.h>
#include <stdio.h>
#include "png.h"
#include "secret_util.h"

#define PNG_BYTES_TO_CHECK 4

int check_png(char *file_path) {
    FILE *file = fopen(file_path, "rb");
    int result = check_png2(file);
    if (file)
        fclose(file);
    return result;
}

int check_png2(FILE *file) {
    png_byte buf[PNG_BYTES_TO_CHECK];
    if (file == NULL)
        return 0;
    rewind(file);
    if (fread(buf, 1, PNG_BYTES_TO_CHECK, file) != PNG_BYTES_TO_CHECK) {
        rewind(file);
        return 0;
    }
    // 对比文件前4个magic字节
    int result = !png_sig_cmp(buf, 0, PNG_BYTES_TO_CHECK);
    // 将文件指针指回文件开头，不影响后续libpng对文件的操作
    rewind(file);
    return result;
}

char adam7_row_interval[7] = {8, 8, 4, 4, 2, 2, 1};
char adam7_row_offset[7] = {0, 4, 0, 2, 0, 1, 0};
char adam7_col_interval[7] = {8, 8, 8, 4, 4, 2, 2};
char adam7_col_offset[7] = {0, 0, 4, 0, 2, 0, 1};

size_t get_color_bytes(unsigned char color_type) {
    switch (color_type) {
        case 0:// 灰度图像
            return 1;
        case 2:// 真色彩图像
            return 3;
        case 4:// 带α通道的灰度图像
            return 2;
        case 6:// 带α通道的真色彩图像
            return 4;
        default:
            return 0;// 目前不支持类型3，索引彩色图像，所以认为颜色字节数为0
    }
}

int col_is_adam7(int index, int pass, unsigned char color_type) {
    if (pass == -1) {
        return 1;
    }
    size_t color_byte = get_color_bytes(color_type);
    if (color_byte == 0) {
        return 0;
    }
    int i = index / color_byte;
    int interval = adam7_row_interval[pass];
    int offset = adam7_row_offset[pass];
    return i % interval == offset;
}

int row_is_adam7(int index, int pass) {
    if (pass == -1) {
        return 1;
    }
    return index % adam7_col_interval[pass] == adam7_col_offset[pass];
}

size_t get_adam7_byte_size(size_t row_byte, int pass, unsigned char color_type) {
    if (pass == -1) {
        return row_byte;
    }
    size_t color_byte = get_color_bytes(color_type);
    if (color_byte == 0) {
        return 0;
    }
    size_t real_size = row_byte / color_byte;
    int interval = adam7_row_interval[pass];
    int offset = adam7_row_offset[pass];
    return (real_size - offset + interval - 1) / interval * color_byte;// 除法向上取整
}