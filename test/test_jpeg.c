//
// Created by jason on 2016/2/19.
//

#include "secret_file.h"

void test_mem_rw(const char *rw_inname, const char *rw_outname, int has_meta) {

}

void test_file_rw(const char *rw_inname, const char *rw_outname, int has_meta) {

}

int main(int argc, char *argv[]) {

    secret_init();

    static const char *error_name = "H:\\img_test\\other.png";
    static const char *rw_inname = "H:\\img_test\\pngtest_rgba.png";
    static const char *rw_outname = "H:\\img_test\\out_pngtest_rgba.png";

    test_mem_rw(rw_inname, rw_outname, 1);
    test_file_rw(rw_inname, rw_outname, 1);

    return 0;
}