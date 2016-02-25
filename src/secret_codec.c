//
// Created by jason on 2016/1/6.
//

#include <string.h>
#include <secret_codec.h>
#include "secret_codec.h"

#ifndef to_odd
#define to_odd(type, data) (data & 0x01 ? data : (type) (data | 0x0001))
#endif

#ifndef to_even
#define to_even(type, data) (data & 0x01 ? (type) (data & 0xfffe) : data)
#endif

#ifndef to_lsb
#define to_lsb(bit, type, data) (bit ? to_odd(type, data) : to_even(type, data))
#endif

static unsigned char mask[8] = {128, 64, 32, 16, 8, 4, 2, 1};// 用于提取byte中的每个bit

static size_t carrier_type_size(SECRET_CARRIER_TYPE data_t) {
    switch (data_t) {
        case S_CHAR:
            return sizeof(char);
        case S_U_CHAR:
            return sizeof(unsigned char);
        case S_SHORT:
            return sizeof(short);
        case S_U_SHORT:
            return sizeof(unsigned short);
        case S_INTEGER:
            return sizeof(int);
        case S_U_INTEGER:
            return sizeof(unsigned int);
        default:
            return 0;
    }
}

static void carrier_set_bit(SECRET_CARRIER_TYPE data_t, void *data_p, int index, int bit) {
    switch (data_t) {
        case S_CHAR: {
            char *c_data_p = data_p;
            c_data_p[index] = to_lsb(bit, char, c_data_p[index]);
            break;
        }
        case S_U_CHAR:{
            unsigned char *uc_data_p = data_p;
            uc_data_p[index] = to_lsb(bit, unsigned char, uc_data_p[index]);
            break;
        }
        case S_SHORT:{
            short *s_data_p = data_p;
            s_data_p[index] = to_lsb(bit, short, s_data_p[index]);
            break;
        }
        case S_U_SHORT:{
            unsigned short *us_data_p = data_p;
            us_data_p[index] = to_lsb(bit, unsigned short, us_data_p[index]);
            break;
        }
        case S_INTEGER:{
            int *i_data_p = data_p;
            i_data_p[index] = to_lsb(bit, int, i_data_p[index]);
            break;
        }
        case S_U_INTEGER:{
            unsigned int *ui_data_p = data_p;
            ui_data_p[index] = to_lsb(bit, unsigned int, ui_data_p[index]);
            break;
        }
    }
}

static int carrier_get_bit(SECRET_CARRIER_TYPE data_t, void *data_p, int index) {
    switch (data_t) {
        case S_CHAR: {
            char *c_data_p = data_p;
            return c_data_p[index] & 0x01;
        }
        case S_U_CHAR:{
            unsigned char *uc_data_p = data_p;
            return uc_data_p[index] & 0x01;
        }
        case S_SHORT:{
            short *s_data_p = data_p;
            return s_data_p[index] & 0x01;
        }
        case S_U_SHORT:{
            unsigned short *us_data_p = data_p;
            return us_data_p[index] & 0x01;
        }
        case S_INTEGER:{
            int *i_data_p = data_p;
            return i_data_p[index] & 0x01;
        }
        case S_U_INTEGER:{
            unsigned int *ui_data_p = data_p;
            return ui_data_p[index] & 0x01;
        }
        default:
            return 0;
    }
}

size_t secret_volume(SECRET_CARRIER_TYPE data_t, size_t data_size) {
    return data_size / 8;
}

int secret_hide(SECRET_CARRIER_TYPE data_t, void *data_p, int data_offset, size_t data_size,
                unsigned char *secret_p, int secret_offset, size_t secret_size,
                secret_remainder *remain, secret_filter *filter) {
    if (!data_p || data_size <= 0) {
        return -1;
    }
    // LSB编码法。每个字节的最后一位，用于存储secret的1bit，即一个字节的data能存储1bit的信息
    // bit流存储方式是，big-endian大端字节序，即优先存储bit位的是一个字节的最高有效位
    size_t remain_size = remain ? remain->size : 0;
    size_t write_data_size = 0;// 已写入的有效字节数
    size_t write_secret_size = 0;// 已写入的secret字节数
    int bit_index = remain_size;// secret字节中第几个bit
    int tmp_bit;
    int i;
    for (i = data_offset; i < (data_size + data_offset); i++) {
        if (filter && filter->is_effective &&
            !(filter->is_effective(data_p, i, filter->param))) {
            continue;
        }
        // 写入secret信息进data
        tmp_bit = secret_p[secret_offset + write_secret_size] & mask[bit_index];
        carrier_set_bit(data_t, data_p, i, tmp_bit);
        bit_index++;
        if (bit_index % 8 == 0) {
            bit_index = 0;
            write_secret_size++;
            if (write_secret_size >= secret_size) {
                // Secret is write-out before data write-out!
                if (remain) {
                    remain->size = 0;
                }
                break;
            }
        }
        write_data_size++;
        if (remain) {
            /** 不需要真的将余数的数据写入remain->data中*/
            remain->size = (write_data_size + remain_size) % 8;
        }
    }
    return write_secret_size;
}

int secret_dig(SECRET_CARRIER_TYPE data_t, void *data_p, int data_offset, size_t data_size,
               unsigned char *secret_p, int secret_offset, size_t secret_size,
               secret_remainder *remain, secret_filter *filter) {
    if (!data_p || data_size <= 0) {
        return -1;
    }
    // LSB编码法。每个字节的最后一位，用于存储secret的1bit，即一个字节的data能存储1bit的信息
    // bit流存储方式是，big-endian大端字节序，即优先存储一个字节的最高有效位
    size_t type_bytes = carrier_type_size(data_t);
    size_t remain_size = remain ? remain->size : 0;
    size_t total_data_size = data_size + remain_size;
    size_t read_data_size = 0;// 已读取的有效字节数
    size_t read_secret_size = 0;// 已解析的secret字节数
    unsigned char tmp_byte = 0;
    unsigned char tmp_bit = 0;
    size_t bit_loc;// 当前解析的secret字节的bit位指针
    void *cur_data;
    int cur_data_index;// 指向当前解析的data单元的指针
    size_t i;
    for (i = 0; i < total_data_size; i++) {
        if (read_secret_size >= secret_size) {
            // Secret is read-out before data read-out!
            if (remain) {
                remain->size = 0;
            }
            break;
        }
        bit_loc = read_data_size % 8;
        if (remain && i < remain_size) {
            cur_data = remain->data;
            cur_data_index = i;
        } else {
            cur_data = data_p;
            cur_data_index = i - remain_size + data_offset;
            if (filter && filter->is_effective &&
                !(filter->is_effective(cur_data, cur_data_index, filter->param))) {
                continue;
            }
            // 将data存入remain
            if (remain && bit_loc < 7) {
                memcpy(remain->data + bit_loc * type_bytes,
                       cur_data + cur_data_index * type_bytes, type_bytes);
                remain->size = bit_loc + 1;
            }
        }
        // 解析remain+data中的secret信息
        tmp_bit = (unsigned char) (carrier_get_bit(data_t, cur_data, cur_data_index));
        tmp_byte = tmp_byte | (tmp_bit << (7 - bit_loc));
        if (bit_loc == 7) {// 满8bit，则可以完整解析一个字节的secret信息了
            secret_p[secret_offset + read_secret_size] = tmp_byte;
            read_secret_size++;
            tmp_byte = 0;
            remain->size = 0;
        }
        read_data_size++;
    }
    return read_secret_size;
}
