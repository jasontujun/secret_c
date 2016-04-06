//
// Created by jason on 2016/1/14.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib/zlib.h>
#include "secret_struct.h"
#include "secret_util.h"

static unsigned char MAGIC[] = "SE";
static unsigned char EMPTY_KEY[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};



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

void secret_destroy(secret *se, int destroy_data){
    if (se) {
        if (destroy_data) {
            free(se->data);
        }
        se->size = 0;
        secret_meta_destroy(se->meta);
        free(se);
    }
}

int secret_get_meta(unsigned char *data,
                    size_t length,
                    secret *result) {
    if (!(result->meta)) {
        return -1;
    }
    // 先验证一下tag，如果不符合格式规定，直接返回-1报错
    if (length >= 2 && memcmp(data, MAGIC, 2) != 0) {
        return -1;
    }
    if (length < SECRET_META_LENGTH) {
        return 0;
    }
    result->size = byte_to_sizet(data + 2, 4);
    memcpy(result->meta->type, data + 6, 2);
    if (memcmp(data + 8, EMPTY_KEY, 16) == 0) {
        result->meta->key = NULL;
    } else {
        result->meta->key = malloc(16);
        memcpy(result->meta->key, data + 8, 16);
    }
    return 1;
}

int secret_create_meta(secret *se, unsigned char **result) {
    if (!se || !result || se->size == 0 || !(se->meta)) {
        return -1;
    }
    unsigned char *meta_bytes = malloc(SECRET_META_LENGTH);
    memcpy(meta_bytes, MAGIC, 2);
    sizet_to_byte(se->size, meta_bytes + 2, 4);
    memcpy(meta_bytes + 6, se->meta->type, 2);
    if (se->meta->key) {
        memcpy(meta_bytes + 8, se->meta->key, 16);
    } else {
        memcpy(meta_bytes + 8, EMPTY_KEY, 16);
    }
    *result = meta_bytes;
    return SECRET_META_LENGTH;
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
        size_t file_size = (size_t) ftell(secret_file);
        if (file_size <= 0) {
            printf("[error]secret_cal_crc():Secret file size is 0!\n");
            fclose(secret_file);
            return 0L;
        }
        rewind(secret_file);
        size_t secret_buf_size = file_size > 1024 ? 1024 : file_size;// 缓冲区的大小
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

int secret_check_crc(unsigned long crc1, unsigned char* crc2) {
    unsigned char crc1_bytes[SECRET_CRC_LENGTH];
    ulong_to_byte(crc1, crc1_bytes, SECRET_CRC_LENGTH);
    return memcmp(crc1_bytes, crc2, SECRET_CRC_LENGTH);
}