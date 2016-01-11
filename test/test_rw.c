//
// Created by jason on 2016/1/7.
//

#include <stdio.h>
#include "secret_image.h"

void test_mem(const char *rw_inname, const char *rw_outname) {
    // write secret into image
    secret *myse = create_secret();
    char str[] = "ABC天气很好okok\n";
    size_t str_size = sizeof(str);
    myse->data = str;
    myse->size = str_size;
    int w_result = write_image(rw_inname, rw_outname, myse, myse->size);
    printf(">>>write result=%d, write content=%s, write bytes=%dbyte\n", w_result, str, str_size);
    free_secret(myse);

    // read secret from image
    secret *se = create_secret();
    se->size = str_size;
    int r_result = read_image(rw_outname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    free_secret(se);
}

void test_file(const char *rw_inname, const char *rw_outname) {
    // write secret into image
    secret *myse = create_secret();
    myse->file_path = "H:\\img_test\\secret\\src.txt";
    int w_result = write_image(rw_inname, rw_outname, myse, 0);
    printf(">>>write result=%d\n", w_result);
    free_secret(myse);
    if (w_result < 0) {
        return;
    }

    // read secret from image
    secret *se = create_secret();
    se->file_path = "H:\\img_test\\secret\\result2.txt";
    se->size = (size_t) w_result;
    int r_result = read_image(rw_outname, se);
    printf(">>>read result=%d, read content=%s\n", r_result, se->data);
    free_secret(se);
}

int main(int argc, char *argv[]) {

    static const char *rw_inname = "H:\\img_test\\pngtest_rgb_interlace_19x19.png";
    static const char *rw_outname = "H:\\img_test\\out_pngtest_rgb_interlace_19x19.png";

//    test_mem(rw_inname, rw_outname);
    test_file(rw_inname, rw_outname);

    return 0;
}
