//
// Created by jason on 2016/1/8.
//

#include <stdio.h>
#include "secret_image.h"

void test_mem(const char *r_inname, size_t secret_size) {
    secret *se = create_secret();
    se->size = secret_size;
    int r_result = read_image_directly(r_inname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    free_secret(se);
}

void test_file(const char *r_inname, size_t secret_size) {
    secret *se = create_secret();
    se->size = secret_size;
    se->file_path = "H:\\img_test\\secret\\result1.txt";
    int r_result = read_image_directly(r_inname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    free_secret(se);
}

int main(int argc, char *argv[]) {

    static const char *error_name = "H:\\img_test\\other.png";
    static const char *r_inname = "H:\\img_test\\out_pngtest_rgba_interlace2.png";

    test_mem(r_inname, 0);
//    test_file(r_inname, 0);

    return 0;
}
