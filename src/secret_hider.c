//
// Created by jason on 2016/1/6.
//

#include "secret_hider.h"

char mask[8] = {-128, 64, 32, 16, 8, 4, 2, 1};// 用于提取byte中的每个bit

unsigned char to_odd(unsigned char data) {
    return data & 0x01 ? data : (unsigned char) ((data < 127) ? data + 1 : data - 1);
}

unsigned char to_even(unsigned char data) {
    return data & 0x01 ? (unsigned char) ((data < 127) ? data + 1 : data - 1) : data;
}

size_t contains_secret_bytes(size_t data_size) {
    return data_size / 8;
}

int write_secret_to_data(unsigned char *data_p, int data_offset, size_t data_size,
                         char *secret_p, int secret_offset, size_t secret_size,
                         remainder *remain, filter* f) {
    if (!data_p || data_size <= 0) {
        return -1;
    }
    size_t remain_size = remain ? remain->size : 0;
    size_t total_real_size = f && f->get_effective_size ?
                             f->get_effective_size(data_size, f->param) + remain_size :
                             data_size + remain_size;// 总的有效字节数
    size_t round_real_size = total_real_size >> 3 << 3;// 能被8整除的最大有效字节数，大于此值的字节将存入remain中
    // 奇偶编码法。每个字节的data，奇数值代表1，偶数值代表0，即一个字节的data能存储1bit的信息
    // bit流存储方式是，big-endian大端字节序，即优先存储bit位的是一个字节的最高有效位
    size_t write_data_size = 0;// 已写入的有效字节数
    size_t write_secret_size = 0;// 已写入的secret字节数
    int bit_index = remain_size;
    unsigned char data_byte;
    char tmp_bit;
    int i;
    for (i = data_offset; i < data_size; i++) {
        if (f && f->is_effective && !(f->is_effective(i, f->param))) {
            continue;
        }
        data_byte = data_p[i];
        tmp_bit = secret_p[secret_offset + write_secret_size] & mask[bit_index];
        if (tmp_bit == 0) {
            // tmp_byte偏移一位变成偶数
            data_byte = to_even(data_byte);
        } else {
            // tmp_byte偏移一位变成奇数
            data_byte = to_odd(data_byte);
        }
        data_p[i] = data_byte;
        bit_index++;
        if (bit_index % 8 == 0) {
            bit_index = 0;
            write_secret_size++;
            if (write_secret_size >= secret_size) {
//                fprintf(STDERR, "Secret is write-out before data write-out! Has write %d bytes secret!\n", write_secret_size);
                if (remain) {
                    remain->size = 0;
                }
                break;
            }
        }
        // 将不足8倍数的data存入remain
        if ((write_data_size + remain_size) >= round_real_size) {
            if (remain) {
                remain->data[write_data_size + remain_size - round_real_size] = data_byte;
                remain->size = write_data_size + remain_size - round_real_size + 1;
            }
        }
        write_data_size++;
    }
    // 如果有效数据总长度是8的倍数，则没有余数，设置余数为0
    if (total_real_size == round_real_size) {
        if (remain) {
            remain->size = 0;
        }
    }
    return write_secret_size;
}

int read_secret_from_data(unsigned char *data_p, int data_offset, size_t data_size,
                          char *secret_p, int secret_offset, size_t secret_size,
                          remainder *remain, filter* f) {
    if (!data_p || data_size <= 0) {
        return -1;
    }
    size_t remain_size = remain ? remain->size : 0;
    size_t total_data_size = data_size + remain_size;
    size_t total_real_size = f && f->get_effective_size ?
                             f->get_effective_size(data_size, f->param) + remain_size :
                             data_size + remain_size;// 总的有效字节数
    size_t round_real_size = total_real_size >> 3 << 3;// 能被8整除的最大有效字节数，大于此值的字节将存入remain中
    // 奇偶解码法。每个字节的data，奇数值代表1，偶数值代表0，即一个字节的data能隐藏1bit的信息
    // bit流存储方式是，big-endian大端字节序，即优先存储一个字节的最高有效位
    size_t read_data_size = 0;// 已读取的有效字节数
    size_t read_secret_size = 0;// 已解析的secret字节数
    char tmp_byte = 0;
    char tmp_bit = 0;
    int bit_loc;
    unsigned char data_byte;
    size_t i;
    for (i = 0; i < total_data_size; i++) {
        if (read_secret_size >= secret_size) {
//            fprintf(STDERR, "Secret is read-out before data read-out! Has read %d bytes secret!\n", read_secret_size);
            if (remain) {
                remain->size = 0;
            }
            break;
        }
        if (i < remain_size) {
            data_byte = remain->data[i];
        } else {
            if (f && f->is_effective && !(f->is_effective(i - remain_size + data_offset, f->param))) {
                continue;
            }
            data_byte = data_p[i - remain_size + data_offset];
        }
        if (read_data_size < round_real_size) {
            // 解析remain+data中的secret信息
//            fprintf(STDERR, "[%d]\t", data_byte);
            bit_loc = read_data_size % 8;
            tmp_bit = (char) (data_byte & 0x01);
            tmp_byte = tmp_byte | (tmp_bit << (7 - bit_loc));
            if (bit_loc == 7) {
                secret_p[secret_offset + read_secret_size] = tmp_byte;
                read_secret_size++;
//                fprintf(STDERR, "<%d>\t", (int) tmp_byte);
                tmp_byte = 0;
            }
        } else {
            // 将不足8倍数的data存入remain
            if (remain) {
                remain->data[read_data_size - round_real_size] = data_byte;
                remain->size = read_data_size - round_real_size + 1;
            }
        }
        read_data_size++;
    }
    // 如果有效数据总长度是8的倍数，则没有余数，设置余数为0
    if (total_real_size == round_real_size) {
        if (remain) {
            remain->size = 0;
        }
    }
//    fprintf(STDERR, "\n\n");
    return read_secret_size;
}
