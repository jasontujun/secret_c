//
// Created by jason on 2016/2/19.
//

#include "secret_file.h"

void test_mem_rw(const char *rw_inname, const char *rw_outname, int has_meta) {
    // write secret into image
    secret *myse = secret_create(has_meta);
    char str[] = "ABCEDFGGHPERTYokok\n";
//    char str[] = "ABC";
    size_t str_size = sizeof(str);
    myse->data = (unsigned char *) str;
    myse->size = str_size;
    int w_result = secret_file_hide(rw_inname, rw_outname, myse);
    printf(">>>write result=%d, write content=%s, write bytes=%dbyte\n", w_result, str, str_size);
    secret_destroy(myse);

    // read secret from image
    secret *se = secret_create(has_meta);
    if (!has_meta) {
        se->size = (size_t) w_result;
    }
    int r_result = secret_file_dig(rw_outname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    secret_destroy(se);
}

void test_file_rw(const char *rw_inname, const char *rw_outname, int has_meta) {
    // write secret into image
    secret *myse = secret_create(has_meta);
    myse->file_path = "H:\\img_test\\secret\\src_jpg.txt";
    int w_result = secret_file_hide(rw_inname, rw_outname, myse);
    printf(">>>write result=%d\n", w_result);
    secret_destroy(myse);
    if (w_result < 0) {
        return;
    }

    // read secret from image
    secret *se = secret_create(has_meta);
    se->file_path = "H:\\img_test\\secret\\result_jpg.txt";
    if (!has_meta) {
        se->size = (size_t) w_result;
    }
    int r_result = secret_file_dig(rw_outname, se);
    printf(">>>read result=%d, check result file: %s\n", r_result, se->file_path);
    secret_destroy(se);
}

int main(int argc, char *argv[]) {

    secret_init();

    static const char *rw_inname = "H:\\img_test\\test_jpg2.jpg";
    static const char *rw_outname = "H:\\img_test\\test_jpg2_out.jpg";

    test_mem_rw(rw_inname, rw_outname, 1);
    test_file_rw(rw_inname, rw_outname, 1);

    return 0;
}