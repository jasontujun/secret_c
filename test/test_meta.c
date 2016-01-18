//
// Created by jason on 2016/1/14.
//

#include <stdio.h>
#include "secret_struct.h"

void test_crc() {
    unsigned int data_size = 1;
    char data[data_size];
    data[0] = 'a';
    unsigned long crc;
    crc = secret_cal_crc2(0L, NULL, 0);
    crc = secret_cal_crc2(crc, data, data_size);
    printf("crc=%x\n", crc);

    char data2[4 + data_size];
    int j;
    for (j = 0; j < data_size; ++j) {
        data2[j] = data[j];
    }
    int i;
    unsigned long tmp;
    for (i = 0; i < 4 ; ++i) {
        tmp = crc >> ((3-i)*8);
        tmp = tmp & 255;
        data2[data_size + i] = (unsigned char) tmp;
    }
    crc = secret_cal_crc2(0L, NULL, 0);
    crc = secret_cal_crc2(crc, data2, 4 + data_size);
    printf("crc2=%x\n", crc);
}

void test_meta() {
    secret *se = secret_create(1);
    se->meta->key = "abcdefghijklmnop";
    se->size = 1024;
    unsigned char *meta = NULL;
    int meta_size;
    if ((meta_size = secret_create_meta(se, &meta)) == -1) {
        printf("secret_create_meta() error!");
        return;
    }
    secret *newse = secret_create(1);
    if (secret_get_meta(meta, (size_t) meta_size, newse) < 0) {
        printf("secret_get_meta() error!");
        return;
    }
    printf("se->size:%d\n", newse->size);
    printf("se->key:%s\n", newse->meta->key);
}

int main(int argc, char *argv[]) {
    test_meta();
    return 0;
}
