//
// Created by jason on 2016/1/14.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib/zlib.h>
#include "secret_struct.h"

static unsigned char MAGIC[] = "SE";

static int mask[4] = {0xFF, 0xFF00, 0xFF0000, 0xFF000000};

/**
 * 将字节数组转换为size_t(采取Big-Endian字节序)。
 * @param bytes 字节数组
 * @param length 字节个数(大于4，等同于4)
 * @return 返回转换后的size_t。
 */
static size_t byte_to_sizet(unsigned char* bytes,
                            size_t length) {
    if (length <= 0) {
        return 0;
    }
    if (length > 4) {
        length = 4;
    }
    size_t result = 0;
    int i;
    for (i= 0; i < length; i++) {
        result |= ((bytes[length - 1 - i] << (8 * i)) & mask[i]);
    }
    return result;
}

/**
 * 将size_t转换为字节数组(采取Big-Endian字节序)。
 * @param integer size_t的值
 * @param bytes 字节数组，转换的结果
 * @param length 字节个数(大于4，等同于4)
 * @return 转换失败返回0；转换成功返回非0。
 */
static int sizet_to_byte(size_t integer,
                         unsigned char* bytes,
                         size_t length) {
    if (length <= 0) {
        return 0;
    }
    if (length > 4) {
        length = 4;
    }
    int i;
    for (i= 0; i < length; i++) {
        bytes[length - 1 - i] = (unsigned char) ((integer & mask[i]) >> (8 * i));
    }
    return 1;
}

static secret_meta *secret_meta_create() {
    secret_meta *meta = malloc(sizeof(secret_meta));
    meta->key = NULL;
    meta->crc = 0;
    memset(meta->type, 0, 2);
    return meta;
}

static void secret_meta_destroy(secret_meta *meta) {
    if (meta) {
        free(meta->key);
        meta->crc = 0;
        free(meta);
    }
}

secret *secret_create(int meta) {
    secret *se = malloc(sizeof(secret));
    se->file_path = NULL;
    se->data = NULL;
    se->size = 0;
    if (meta) {
        se->meta = secret_meta_create();
    } else {
        se->meta = NULL;
    }
    return se;
}

void secret_destroy(secret *se){
    if (se) {
        free(se->file_path);
        free(se->data);
        se->size = 0;
        secret_meta_destroy(se->meta);
        free(se);
    }
}

int secret_check_meta(unsigned char *data,
                      size_t length) {
    if (length < 2) {
        return 0;
    }
    return memcmp(data, MAGIC, 2) ? -1 : 1;
}

int secret_get_meta(unsigned char *data,
                    size_t length,
                    secret *result) {
    if (!(result->meta)) {
        return -1;
    }
    if (secret_check_meta(data, length) < 0) {
        return -1;
    }
    if (length < 8) {
        return 0;
    }
    result->size = byte_to_sizet(data + 2, 4);
    memcpy(result->meta->type, data + 6, 2);
    if (result->meta->type[1] & 0x01) {
        if (length < 24) {
            return 0;
        } else {
            result->meta->key = malloc(16);
            memcpy(result->meta->key, data + 8, 16);
            return 24;
        }
    } else {
        return 8;
    }
}

int secret_create_meta(secret *se, unsigned char **result) {
    if (se->size == 0 || !(se->meta)) {
        return -1;
    }
    size_t size = se->meta->key ? 24 : 8;
    unsigned char *meta_bytes = malloc(size);
    memcpy(meta_bytes, MAGIC, 2);
    sizet_to_byte(se->size, meta_bytes + 2, 4);
    if (se->meta->key) {
        se->meta->type[1] = se->meta->type[1] | ((unsigned char)0x01);
        memcpy(meta_bytes + 6, se->meta->type, 2);
        memcpy(meta_bytes + 8, se->meta->key, 16);
    } else {
        se->meta->type[1] = se->meta->type[1] & ((unsigned char)0xfe);
        memcpy(meta_bytes + 6, se->meta->type, 2);
    }
    *result = meta_bytes;
    return size;
}

unsigned long secret_cal_crc2(unsigned long crc,
                              unsigned char *data,
                              size_t length) {
    return crc32(crc, data, length);
}

unsigned long secret_cal_crc(secret *se) {
    unsigned long crc = 0;
    if (se->file_path) {
        FILE * secret_file = NULL;
        if ((secret_file = fopen(se->file_path, "rb")) == NULL) {
            printf("[error]secret_cal_crc():Could not find secret file %s\n", se->file_path);
            return 0L;
        }
        fseek(secret_file , 0 , SEEK_END);
        size_t real_size = (size_t) ftell(secret_file);// 文件大小，也就是secret真正的字节大小
        if (real_size <= 0) {
            printf("[error]secret_cal_crc():Secret file size is 0!\n");
            fclose(secret_file);
            return 0L;
        }
        rewind(secret_file);
        size_t secret_buf_size = real_size > 1024 ? 1024 : real_size;// 缓冲区的大小
        unsigned char *secret_buf = malloc(secret_buf_size);
        size_t read_result;
        while (!feof(secret_file)) {
            read_result = fread(secret_buf, 1, secret_buf_size, secret_file);
            if (ferror(secret_file)) {
                printf("[error]secret_cal_crc():Secret file read error!\n");
                return 0L;
            }
            crc = secret_cal_crc2(crc, secret_buf, read_result);
        }
    } else {
        if (!(se->data) || se->size <= 0) {
            return 0L;
        }
        crc = secret_cal_crc2(crc, se->data, se->size);
    }
    return crc;
}