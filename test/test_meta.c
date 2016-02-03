//
// Created by jason on 2016/1/14.
//

#include <stdio.h>
#include <stdlib.h>
#include "secret_struct.h"
#include "secret_util.h"

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

void test_data_source() {
    char *buf = malloc(20);

    // 读测试
    char* mem = "abc";
    data_source *s1 = create_memory_data_source((unsigned char *) mem, 3);
    FILE *f1 = fopen("H:\\img_test\\meta\\aa.txt", "rb");
    data_source *s2 = create_file_data_source(f1, -1);
    data_source *ss[2] = {s1, s2};
    multi_data_source *ms = create_multi_data_source(ss,2);
    int readsize = ms->read(ms, 20, buf);
    printf("read size=%d\n", readsize);
    printf("read result=%s\n", buf);
    destroy_multi_data_source(ms);
    fclose(f1);
    if (readsize < 0) {
        printf("read error!!\n");
        free(buf);
        return;
    }

    // 写测试
    char *mem2 = malloc(3);
    data_source *s3 = create_memory_data_source((unsigned char *) mem2, 3);
    FILE *f2 = fopen("H:\\img_test\\meta\\bb.txt", "wb");
    data_source *s4 = create_file_data_source(f2, 30);
    data_source *ss2[2] = {s3, s4};
    multi_data_source *ms2 = create_multi_data_source(ss2,2);
    int writesize = ms2->write(ms2, (size_t) readsize, buf);
    printf("write size=%d\n", writesize);
    printf("write result=%s\n", mem2);
    destroy_multi_data_source(ms2);
    fclose(f2);
    free(buf);
    if (writesize < 0) {
        printf("write error!!\n");
        return;
    }

    // 移动测试
    char *mem3 = "opqrstu";
    data_source *s10 = create_memory_data_source((unsigned char *) mem3, 7);
    FILE *f3 = fopen("H:\\img_test\\meta\\aa.txt", "rb");
    data_source *s11 = create_file_data_source(f3, -1);
    data_source *ss3[2] = {s10, s11};
    multi_data_source *ms3 = create_multi_data_source(ss3, 2);
    size_t total_size = (size_t) ms3->size(ms3);
    char *buf_all = malloc(total_size);
    ms3->read(ms3, total_size, buf_all);
    printf("total data=%s\n", buf_all);
    free(buf_all);
    long movesize = 3;
    ms3->move(ms3, movesize);
    printf("move size=%d\n", movesize);
    char *buf2 = malloc(20);
    int readsize2 = ms3->read(ms3, 5, buf2);
    printf("read size=%d\n", readsize2);
    printf("move-and-read result=%s\n", buf2);
    destroy_multi_data_source(ms3);
    fclose(f3);
    free(buf2);
}

int main(int argc, char *argv[]) {
//    test_crc()
//    test_meta();
    test_data_source();
    return 0;
}
