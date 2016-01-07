//
// Created by jason on 2016/1/7.
//

#include <stdio.h>
#include "png.h"
#include "secret_image.h"

#define STDERR stdout   /* For DOS */

int main(int argc, char *argv[]) {

    static const char *rw_inname = "H:\\img_test\\pngtest_rgba_interlace2.png";
    static const char *rw_outname = "H:\\img_test\\out_pngtest_rgba_interlace2.png";

    fprintf(STDERR, "\n Testing libpng version %s\n", PNG_LIBPNG_VER_STRING);
    fprintf(STDERR, "%s", png_get_copyright(NULL));

    // write secret into image
    secret *myse = create_secret();
    char str[] = "ABC天气很好okok\n";
    size_t str_size = sizeof(str);
    myse->data = str;
    myse->size = str_size;
    int w_result = write_image(rw_inname, rw_outname, myse, myse->size);
    fprintf(STDERR, ">>>write result=%d, write content=%s, write bytes=%dbyte\n", w_result, str, str_size);
    free_secret(myse);

    // read secret from image
    secret *se = create_secret();
    se->size = str_size;
    int r_result = read_image(rw_outname, se);
    fprintf(STDERR, ">>>read result=%d, read content=%s\n", r_result, se->data);
    free_secret(se);

#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    fprintf(STDERR, " Current memory allocation: %10d bytes\n", current_allocation);
    fprintf(STDERR, " Maximum memory allocation: %10d bytes\n", maximum_allocation);
    fprintf(STDERR, " Total   memory allocation: %10d bytes\n", total_allocation);
    fprintf(STDERR, "     Number of allocations: %10d\n", num_allocations);
#endif

    return 0;
}
