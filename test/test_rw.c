//
// Created by jason on 2016/1/7.
//

#include <stdio.h>
#include "secret_image.h"

void test_mem(const char *rw_inname, const char *rw_outname, int has_meta) {
    // write secret into image
    secret *myse = secret_create(has_meta);
    char str[] = "ABC天气很好okok\n";
    size_t str_size = sizeof(str);
    myse->data = (unsigned char *) str;
    myse->size = str_size;
    int w_result = secret_image_hide(rw_inname, rw_outname, myse);
    printf(">>>write result=%d, write content=%s, write bytes=%dbyte\n", w_result, str, str_size);
    secret_destroy(myse);

    // read secret from image
    secret *se = secret_create(has_meta);
    if (!has_meta) {
        se->size = (size_t) w_result;
    }
    int r_result = secret_image_dig(rw_outname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    secret_destroy(se);
}

void test_file(const char *rw_inname, const char *rw_outname, int has_meta) {
    // write secret into image
    secret *myse = secret_create(has_meta);
    myse->file_path = "H:\\img_test\\secret\\src.txt";
    int w_result = secret_image_hide(rw_inname, rw_outname, myse);
    printf(">>>write result=%d\n", w_result);
    secret_destroy(myse);
    if (w_result < 0) {
        return;
    }

    // read secret from image
    secret *se = secret_create(has_meta);
    se->file_path = "H:\\img_test\\secret\\result5.txt";
    if (!has_meta) {
        se->size = (size_t) w_result;
    }
    int r_result = secret_image_dig(rw_outname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    secret_destroy(se);
}

int main(int argc, char *argv[]) {

    static const char *error_name = "H:\\img_test\\other.png";
    static const char *rw_inname = "H:\\img_test\\pngtest_rgba.png";
    static const char *rw_outname = "H:\\img_test\\out_pngtest_rgba.png";

    test_mem(rw_inname, rw_outname, 1);
    test_file(rw_inname, rw_outname, 1);

    return 0;
}
