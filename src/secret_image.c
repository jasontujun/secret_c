//
// Created by jason on 2016/1/6.
//

#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include "png.h"
#include "secret_image.h"
#include "secret_hider.h"
#include "secret_util.h"


/* Defined so I can write to a file on gui/windowing platforms */
/*  #define STDERR stderr  */
#define STDERR stdout   /* For DOS */

/* Makes youmetest verbose so we can find problems. */
#ifndef PNG_DEBUG
#  define PNG_DEBUG 2
#endif

#if PNG_DEBUG > 1
#  define youmetest_debug(m)        ((void)fprintf(stderr, m "\n"))
#  define youmetest_debug1(m,p1)    ((void)fprintf(stderr, m "\n", p1))
#  define youmetest_debug2(m,p1,p2) ((void)fprintf(stderr, m "\n", p1, p2))
#else
#  define youmetest_debug(m)        ((void)0)
#  define youmetest_debug1(m,p1)    ((void)0)
#  define youmetest_debug2(m,p1,p2) ((void)0)
#endif

#if !PNG_DEBUG
#  define SINGLE_ROWBUF_ALLOC  /* Makes buffer overruns easier to nail */
#endif

#define FCLOSE(file) fclose(file)

static int verbose = 1;
static int strict = 0;
static int relaxed = 0;


/* START of code to validate memory allocation and deallocation */
#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG

/* Allocate memory.  For reasonable files, size should never exceed
 * 64K.  However, zlib may allocate more than 64K if you don't tell
 * it not to.  See zconf.h and png.h for more information.  zlib does
 * need to allocate exactly 64K, so whatever you call here must
 * have the ability to do that.
 *
 * This piece of code can be compiled to validate max 64K allocations
 * by setting MAXSEG_64K in zlib zconf.h *or* PNG_MAX_MALLOC_64K.
 */
typedef struct memory_information
{
    png_alloc_size_t          size;
    png_voidp                 pointer;
    struct memory_information *next;
} memory_information;
typedef memory_information *memory_infop;

static memory_infop pinformation = NULL;
static int current_allocation = 0;
static int maximum_allocation = 0;
static int total_allocation = 0;
static int num_allocations = 0;

png_voidp PNGCBAPI png_debug_malloc PNGARG((png_structp png_ptr,
        png_alloc_size_t size));
void PNGCBAPI png_debug_free PNGARG((png_structp png_ptr, png_voidp ptr));

png_voidp
PNGCBAPI png_debug_malloc(png_structp png_ptr, png_alloc_size_t size)
{

    /* png_malloc has already tested for NULL; png_create_struct calls
     * png_debug_malloc directly, with png_ptr == NULL which is OK
     */

    if (size == 0)
        return (NULL);

    /* This calls the library allocator twice, once to get the requested
       buffer and once to get a new free list entry. */
    {
        /* Disable malloc_fn and free_fn */
        memory_infop pinfo;
        png_set_mem_fn(png_ptr, NULL, NULL, NULL);
        pinfo = (memory_infop)png_malloc(png_ptr,
                                         (sizeof *pinfo));
        pinfo->size = size;
        current_allocation += size;
        total_allocation += size;
        num_allocations ++;

        if (current_allocation > maximum_allocation)
            maximum_allocation = current_allocation;

        pinfo->pointer = png_malloc(png_ptr, size);
        /* Restore malloc_fn and free_fn */

        png_set_mem_fn(png_ptr,
                       NULL, png_debug_malloc, png_debug_free);

        if (size != 0 && pinfo->pointer == NULL)
        {
            current_allocation -= size;
            total_allocation -= size;
            png_error(png_ptr,
                      "out of memory in pngtest->png_debug_malloc");
        }

        pinfo->next = pinformation;
        pinformation = pinfo;
        /* Make sure the caller isn't assuming zeroed memory. */
        memset(pinfo->pointer, 0xdd, pinfo->size);

        if (verbose != 0)
            printf("png_malloc %lu bytes at %p\n", (unsigned long)size,
                   pinfo->pointer);

        return (png_voidp)(pinfo->pointer);
    }
}

/* Free a pointer.  It is removed from the list at the same time. */
void PNGCBAPI
png_debug_free(png_structp png_ptr, png_voidp ptr)
{
    if (png_ptr == NULL)
        fprintf(STDERR, "NULL pointer to png_debug_free.\n");

    if (ptr == 0)
    {
#if 0 /* This happens all the time. */
        fprintf(STDERR, "WARNING: freeing NULL pointer\n");
#endif
        return;
    }

    /* Unlink the element from the list. */
    {
        memory_infop *ppinfo = &pinformation;

        for (;;)
        {
            memory_infop pinfo = *ppinfo;

            if (pinfo->pointer == ptr)
            {
                *ppinfo = pinfo->next;
                current_allocation -= pinfo->size;
                if (current_allocation < 0)
                    fprintf(STDERR, "Duplicate free of memory\n");
                /* We must free the list element too, but first kill
                   the memory that is to be freed. */
                memset(ptr, 0x55, pinfo->size);
                if (pinfo != NULL)
                    free(pinfo);
                pinfo = NULL;
                break;
            }

            if (pinfo->next == NULL)
            {
                fprintf(STDERR, "Pointer %p not found\n", ptr);
                break;
            }

            ppinfo = &pinfo->next;
        }
    }

    /* Finally free the data. */
    if (verbose != 0)
        printf("Freeing %p\n", ptr);

    if (ptr != NULL)
        free(ptr);
    ptr = NULL;
}
#endif /* USER_MEM && DEBUG */
/* END of code to test memory allocation/deallocation */




secret * create_secret() {
    secret *se = malloc(sizeof(secret));
    se->file_path = NULL;
    se->data = NULL;
    se->size = 0;
    return se;
}

void free_secret(secret *se){
    if (se) {
        free(se->file_path);
        free(se->data);
        se->size = 0;
        free(se);
    }
}

/**
 * return secret_max_size(in byte) in a png_image.
 * if error, return 0.
 */
size_t get_secret_max_size_from_png(png_uint_32 width, png_uint_32 height, png_byte color_type) {
    return width * height * get_color_bytes(color_type) / 8;
}

typedef struct _interlace_param {
    int pass;// 当前扫描的是第几层
    png_byte color_type;// 颜色类型
}interlace_param;

// 判断当前索引位置的数据是否有效
int is_effective(int index, void *param) {
    interlace_param *p = param;
    return col_is_adam7(index, p->pass, p->color_type);
}

// 获取给定数据内有效数据的总数
size_t get_effective_size(size_t data_size, void *param) {
    interlace_param *p = param;
    return get_adam7_byte_size(data_size, p->pass, p->color_type);
}

int read_image(const char *image_file, secret *se) {
    if (!image_file || !se) {
        return -1;
    }

    static png_FILE_p fpin;
    png_structp read_ptr;
    png_infop read_info_ptr, end_info_ptr;

    int num_pass = 1, pass;
    png_bytep row_buf = NULL;

    if ((fpin = fopen(image_file, "rb")) == NULL) {
        fprintf(STDERR, "Could not find input file %s\n", image_file);
        return -2;
    }

    youmetest_debug("Allocating read structures");
#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    read_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL,
                                        NULL, NULL, NULL, png_debug_malloc, png_debug_free);
#else
    read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
#endif
    youmetest_debug("Allocating read_info and end_info structures");
    read_info_ptr = png_create_info_struct(read_ptr);
    end_info_ptr = png_create_info_struct(read_ptr);


#ifdef PNG_SETJMP_SUPPORTED
    youmetest_debug("Setting jmpbuf for read struct");
    if (setjmp(png_jmpbuf(read_ptr))) {
        fprintf(STDERR, "%s: libpng read error\n", image_file);
        png_free(read_ptr, row_buf);
        row_buf = NULL;
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        FCLOSE(fpin);
        return -3;
    }
#endif

    /* Allow application (pngtest) errors and warnings to pass */
    png_set_benign_errors(read_ptr, 1);


    youmetest_debug("Initializing input streams");
    png_init_io(read_ptr, fpin);

    youmetest_debug("Reading info struct");
    png_read_info(read_ptr, read_info_ptr);

    png_uint_32 width = png_get_image_width(read_ptr,read_info_ptr);
    png_uint_32 height = png_get_image_height(read_ptr,read_info_ptr);
    png_byte bit_depth = png_get_bit_depth(read_ptr,read_info_ptr);
    png_byte color_type = png_get_color_type(read_ptr,read_info_ptr);
    png_byte interlace_type = png_get_interlace_type(read_ptr,read_info_ptr);
    png_byte compression_type = png_get_compression_type(read_ptr,read_info_ptr);
    png_byte filter_type = png_get_filter_type(read_ptr,read_info_ptr);
    youmetest_debug1("image width %d", width);
    youmetest_debug1("image height %d", height);
    youmetest_debug1("image bit_depth %d", bit_depth);
    youmetest_debug1("image color_type %d", color_type);
    youmetest_debug1("image interlace_type %d", interlace_type);
    youmetest_debug1("image compression_type %d", compression_type);
    youmetest_debug1("image filter_type %d", filter_type);

#ifdef PNG_READ_INTERLACING_SUPPORTED
    num_pass = png_set_interlace_handling(read_ptr);
    if (num_pass != 1 && num_pass != 7) {
        youmetest_debug1("Image interlace_pass error!! interlace_pass=%d, cannot hide secret!", num_pass);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        FCLOSE(fpin);
        return -4;
    }
#endif

    size_t max_secret_size = get_secret_max_size_from_png(width, height, color_type);
    if (max_secret_size == 0) {
        youmetest_debug1("Image color_type error!! color_type=%d, cannot hide secret!", color_type);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        FCLOSE(fpin);
        return -5;
    }

    size_t expect_size = se->size;
    if (expect_size > 0 && max_secret_size < expect_size) {
        youmetest_debug1("Max secret size less then %d!", expect_size);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        FCLOSE(fpin);
        return -6;
    }

    png_size_t row_buf_size = png_get_rowbytes(read_ptr, read_info_ptr);
    row_buf = (png_bytep)png_malloc(read_ptr, row_buf_size);
    youmetest_debug1("Allocating row buffer...row_buf_size = %d", row_buf_size);

    se->size = expect_size <= 0 ? max_secret_size : expect_size;
    se->data = malloc(se->size);
    youmetest_debug1("Allocating secret buffer...secret_size = %d", se->size);

    remainder remain = {
            .size=0
    };
    interlace_param param = {
            .color_type = color_type
    };
    filter f = {
            .param = &param,
            .is_effective = is_effective,
            .get_effective_size = get_effective_size
    };

    int y;
    int read_result;
    size_t secret_read = 0;
    for (pass = 0; pass < num_pass; pass++) {
        fprintf(STDERR, "Reading row data for pass %d\n", pass);
        for (y = 0; y < height; y++) {
            // read rgba info
            png_read_row(read_ptr, row_buf, NULL);
            // if is not useful adam7 row, skip
            if (!row_is_adam7(y, num_pass > 1 ? pass : -1)) {
                continue;
            }
            // parse secret!
            param.pass = num_pass > 1 ? pass : -1;
            read_result = read_secret_from_data(row_buf, 0, row_buf_size,
                                                se->data, secret_read,
                                                se->size - secret_read,
                                                &remain, &f);
            if (read_result >= 0) {
                secret_read += read_result;
                if (secret_read == se->size) {// 读取的secret内容已满，达到指定的长度，退出循环
                    youmetest_debug("Parse secret finish!!");
                    break;
                }
            }
        }
        if (secret_read == se->size) {// 读取的secret内容已满，达到指定的长度，退出循环
            break;
        }
    }

    youmetest_debug("Destroying row_buf for read_ptr");
    png_free(read_ptr, row_buf);
    row_buf = NULL;
    youmetest_debug("destroying read_ptr, read_info_ptr, end_info_ptr");
    png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);

    FCLOSE(fpin);
    se->size = secret_read;
    return secret_read;
}


int write_image(const char *image_input_file,
                const char *image_output_file,
                secret *se, size_t min_size) {
    if (!image_input_file || !image_output_file || !se || se->size <= 0) {
        return -1;
    }

    static png_FILE_p fpin;
    static png_FILE_p fpout;  /* "static" prevents setjmp corruption */
    png_structp read_ptr;
    png_infop read_info_ptr, end_info_ptr;
    png_structp write_ptr;
    png_infop write_info_ptr, write_end_info_ptr;
    int interlace_preserved = 1;

    png_bytep row_buf = NULL;
    int num_pass = 1, pass;

    if ((fpin = fopen(image_input_file, "rb")) == NULL)
    {
        fprintf(STDERR, "Could not find input file %s\n", image_input_file);
        return -2;
    }

    if ((fpout = fopen(image_output_file, "wb")) == NULL)
    {
        fprintf(STDERR, "Could not open output file %s\n", image_output_file);
        FCLOSE(fpin);
        return -3;
    }

    youmetest_debug("Allocating read and write structures");
#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    read_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL,
                                        NULL, NULL, NULL, png_debug_malloc, png_debug_free);
#else
    read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
#endif

#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    write_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, NULL,
                                          NULL, NULL, NULL, png_debug_malloc, png_debug_free);
#else
    write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
#endif

    youmetest_debug("Allocating read_info, write_info and end_info structures");
    read_info_ptr = png_create_info_struct(read_ptr);
    end_info_ptr = png_create_info_struct(read_ptr);
    write_info_ptr = png_create_info_struct(write_ptr);
    write_end_info_ptr = png_create_info_struct(write_ptr);

#ifdef PNG_SETJMP_SUPPORTED
    youmetest_debug("Setting jmpbuf for read struct");
    if (setjmp(png_jmpbuf(read_ptr)))
    {
        fprintf(STDERR, "%s -> %s: libpng read error\n", image_input_file, image_output_file);
        png_free(read_ptr, row_buf);
        row_buf = NULL;
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        FCLOSE(fpin);
        FCLOSE(fpout);
        return -4;
    }

    youmetest_debug("Setting jmpbuf for write struct");

    if (setjmp(png_jmpbuf(write_ptr)))
    {
        fprintf(STDERR, "%s -> %s: libpng write error\n", image_input_file, image_output_file);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        FCLOSE(fpin);
        FCLOSE(fpout);
        return -5;
    }
#endif

    if (strict != 0)
    {
        /* Treat png_benign_error() as errors on read */
        png_set_benign_errors(read_ptr, 0);
        /* Treat them as errors on write */
        png_set_benign_errors(write_ptr, 0);

        /* if strict is not set, then app warnings and errors are treated as
         * warnings in release builds, but not in unstable builds; this can be
         * changed with '--relaxed'.
         */
    }
    else if (relaxed != 0)
    {
        /* Allow application (pngtest) errors and warnings to pass */
        png_set_benign_errors(read_ptr, 1);
        png_set_benign_errors(write_ptr, 1);
    }

    youmetest_debug("Initializing input and output streams");
#ifdef PNG_STDIO_SUPPORTED
    png_init_io(read_ptr, fpin);
    png_init_io(write_ptr, fpout);
#else
    png_set_read_fn(read_ptr, (png_voidp)fpin, pngtest_read_data);
    png_set_write_fn(write_ptr, (png_voidp)fpout,  pngtest_write_data,
#    ifdef PNG_WRITE_FLUSH_SUPPORTED
            pngtest_flush);
#    else
                     NULL);
#    endif
#endif

    png_set_write_status_fn(write_ptr, NULL);
    png_set_read_status_fn(read_ptr, NULL);

#ifdef PNG_SET_UNKNOWN_CHUNKS_SUPPORTED
    /* Preserve all the unknown chunks, if possible.  If this is disabled then,
     * even if the png_{get,set}_unknown_chunks stuff is enabled, we can't use
     * libpng to *save* the unknown chunks on read (because we can't switch the
     * save option on!)
     *
     * Notice that if SET_UNKNOWN_CHUNKS is *not* supported read will discard all
     * unknown chunks and write will write them all.
     */
#ifdef PNG_SAVE_UNKNOWN_CHUNKS_SUPPORTED
    png_set_keep_unknown_chunks(read_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);
#endif
#ifdef PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
    png_set_keep_unknown_chunks(write_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);
#endif
#endif

    youmetest_debug("Reading info struct");
    png_read_info(read_ptr, read_info_ptr);

    png_uint_32 width = png_get_image_width(read_ptr,read_info_ptr);
    png_uint_32 height = png_get_image_height(read_ptr,read_info_ptr);
    png_byte bit_depth = png_get_bit_depth(read_ptr,read_info_ptr);
    png_byte color_type = png_get_color_type(read_ptr,read_info_ptr);
    png_byte interlace_type = png_get_interlace_type(read_ptr,read_info_ptr);
    png_byte compression_type = png_get_compression_type(read_ptr,read_info_ptr);
    png_byte filter_type = png_get_filter_type(read_ptr,read_info_ptr);
    youmetest_debug1("image width %d", width);
    youmetest_debug1("image height %d", height);
    youmetest_debug1("image bit_depth %d", bit_depth);
    youmetest_debug1("image color_type %d", color_type);
    youmetest_debug1("image interlace_type %d", interlace_type);
    youmetest_debug1("image compression_type %d", compression_type);
    youmetest_debug1("image filter_type %d", filter_type);

#ifdef PNG_READ_INTERLACING_SUPPORTED
    num_pass = png_set_interlace_handling(read_ptr);
    if (num_pass != 1 && num_pass != 7) {
        youmetest_debug1("Image interlace_pass error!! interlace_pass=%d, cannot hide secret!", num_pass);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        FCLOSE(fpin);
        FCLOSE(fpout);
        return -6;
    }
#endif

    size_t max_secret_size = get_secret_max_size_from_png(width, height, color_type);
    if (max_secret_size == 0) {
        youmetest_debug1("Image color_type error!! color_type=%d, cannot hide secret!", color_type);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        FCLOSE(fpin);
        FCLOSE(fpout);
        return -7;
    }

    if (min_size > 0 && max_secret_size < min_size) {
        youmetest_debug1("Max secret size less then %d!", min_size);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        FCLOSE(fpin);
        FCLOSE(fpout);
        return -8;
    }

    youmetest_debug("Transferring info struct");
    {
        png_uint_32 width2, height2;
        int bit_depth2, color_type2, interlace_type2, compression_type2, filter_type2;
        if (png_get_IHDR(read_ptr, read_info_ptr, &width2, &height2, &bit_depth2,
                         &color_type2, &interlace_type2, &compression_type2, &filter_type2) != 0)
        {
            png_set_IHDR(write_ptr, write_info_ptr, width2, height2, bit_depth2,
                         color_type2, interlace_type2, compression_type2, filter_type2);
        }
    }
#ifdef PNG_FIXED_POINT_SUPPORTED
#ifdef PNG_cHRM_SUPPORTED
    {
        png_fixed_point white_x, white_y, red_x, red_y, green_x, green_y, blue_x,
                blue_y;
        if (png_get_cHRM_fixed(read_ptr, read_info_ptr, &white_x, &white_y,
                               &red_x, &red_y, &green_x, &green_y, &blue_x, &blue_y) != 0)
        {
            png_set_cHRM_fixed(write_ptr, write_info_ptr, white_x, white_y, red_x,
                               red_y, green_x, green_y, blue_x, blue_y);
        }
    }
#endif
#ifdef PNG_gAMA_SUPPORTED
    {
        png_fixed_point gamma;
        if (png_get_gAMA_fixed(read_ptr, read_info_ptr, &gamma) != 0)
            png_set_gAMA_fixed(write_ptr, write_info_ptr, gamma);
    }
#endif
#else /* Use floating point versions */
    #ifdef PNG_FLOATING_POINT_SUPPORTED
    #ifdef PNG_cHRM_SUPPORTED
   {
      double white_x, white_y, red_x, red_y, green_x, green_y, blue_x, blue_y;
      if (png_get_cHRM(read_ptr, read_info_ptr, &white_x, &white_y, &red_x,
         &red_y, &green_x, &green_y, &blue_x, &blue_y) != 0)
      {
         png_set_cHRM(write_ptr, write_info_ptr, white_x, white_y, red_x,
            red_y, green_x, green_y, blue_x, blue_y);
      }
   }
#endif
#ifdef PNG_gAMA_SUPPORTED
   {
      double gamma;
      if (png_get_gAMA(read_ptr, read_info_ptr, &gamma) != 0)
         png_set_gAMA(write_ptr, write_info_ptr, gamma);
   }
#endif
#endif /* Floating point */
#endif /* Fixed point */
#ifdef PNG_iCCP_SUPPORTED
    {
        png_charp name;
        png_bytep profile;
        png_uint_32 proflen;
        int compression_type2;
        if (png_get_iCCP(read_ptr, read_info_ptr, &name, &compression_type2, &profile, &proflen) != 0)
        {
            png_set_iCCP(write_ptr, write_info_ptr, name, compression_type2,
                         profile, proflen);
        }
    }
#endif
#ifdef PNG_sRGB_SUPPORTED
    {
        int intent;
        if (png_get_sRGB(read_ptr, read_info_ptr, &intent) != 0)
            png_set_sRGB(write_ptr, write_info_ptr, intent);
    }
#endif
    {
        png_colorp palette;
        int num_palette;
        if (png_get_PLTE(read_ptr, read_info_ptr, &palette, &num_palette) != 0)
            png_set_PLTE(write_ptr, write_info_ptr, palette, num_palette);
    }
#ifdef PNG_bKGD_SUPPORTED
    {
        png_color_16p background;
        if (png_get_bKGD(read_ptr, read_info_ptr, &background) != 0)
        {
            png_set_bKGD(write_ptr, write_info_ptr, background);
        }
    }
#endif
#ifdef PNG_hIST_SUPPORTED
    {
        png_uint_16p hist;
        if (png_get_hIST(read_ptr, read_info_ptr, &hist) != 0)
            png_set_hIST(write_ptr, write_info_ptr, hist);
    }
#endif
#ifdef PNG_oFFs_SUPPORTED
    {
        png_int_32 offset_x, offset_y;
        int unit_type;
        if (png_get_oFFs(read_ptr, read_info_ptr, &offset_x, &offset_y, &unit_type) != 0)
        {
            png_set_oFFs(write_ptr, write_info_ptr, offset_x, offset_y, unit_type);
        }
    }
#endif
#ifdef PNG_pCAL_SUPPORTED
    {
        png_charp purpose, units;
        png_charpp params;
        png_int_32 X0, X1;
        int type, nparams;
        if (png_get_pCAL(read_ptr, read_info_ptr, &purpose, &X0, &X1, &type,
                         &nparams, &units, &params) != 0)
        {
            png_set_pCAL(write_ptr, write_info_ptr, purpose, X0, X1, type,
                         nparams, units, params);
        }
    }
#endif
#ifdef PNG_pHYs_SUPPORTED
    {
        png_uint_32 res_x, res_y;
        int unit_type;
        if (png_get_pHYs(read_ptr, read_info_ptr, &res_x, &res_y,
                         &unit_type) != 0)
            png_set_pHYs(write_ptr, write_info_ptr, res_x, res_y, unit_type);
    }
#endif
#ifdef PNG_sBIT_SUPPORTED
    {
        png_color_8p sig_bit;
        if (png_get_sBIT(read_ptr, read_info_ptr, &sig_bit) != 0)
            png_set_sBIT(write_ptr, write_info_ptr, sig_bit);
    }
#endif
#ifdef PNG_sCAL_SUPPORTED
#if defined(PNG_FLOATING_POINT_SUPPORTED) && \
   defined(PNG_FLOATING_ARITHMETIC_SUPPORTED)
    {
        int unit;
        double scal_width, scal_height;
        if (png_get_sCAL(read_ptr, read_info_ptr, &unit, &scal_width, &scal_height) != 0)
        {
            png_set_sCAL(write_ptr, write_info_ptr, unit, scal_width, scal_height);
        }
    }
#else
    #ifdef PNG_FIXED_POINT_SUPPORTED
   {
      int unit;
      png_charp scal_width, scal_height;

      if (png_get_sCAL_s(read_ptr, read_info_ptr, &unit, &scal_width,
          &scal_height) != 0)
      {
         png_set_sCAL_s(write_ptr, write_info_ptr, unit, scal_width,
             scal_height);
      }
   }
#endif
#endif
#endif
#ifdef PNG_TEXT_SUPPORTED
    {
        png_textp text_ptr;
        int num_text;
        if (png_get_text(read_ptr, read_info_ptr, &text_ptr, &num_text) > 0)
        {
            youmetest_debug1("Handling %d iTXt/tEXt/zTXt chunks", num_text);
            png_set_text(write_ptr, write_info_ptr, text_ptr, num_text);
        }
    }
#endif
#ifdef PNG_tIME_SUPPORTED
    {
        png_timep mod_time;
        if (png_get_tIME(read_ptr, read_info_ptr, &mod_time) != 0)
        {
            png_set_tIME(write_ptr, write_info_ptr, mod_time);
        }
    }
#endif
#ifdef PNG_tRNS_SUPPORTED
    {
        png_bytep trans_alpha;
        int num_trans;
        png_color_16p trans_color;
        if (png_get_tRNS(read_ptr, read_info_ptr, &trans_alpha, &num_trans,
                         &trans_color) != 0)
        {
            int sample_max = (1 << bit_depth);
            /* libpng doesn't reject a tRNS chunk with out-of-range samples */
            if (!((color_type == PNG_COLOR_TYPE_GRAY &&
                   (int)trans_color->gray > sample_max) ||
                  (color_type == PNG_COLOR_TYPE_RGB &&
                   ((int)trans_color->red > sample_max ||
                    (int)trans_color->green > sample_max ||
                    (int)trans_color->blue > sample_max))))
                png_set_tRNS(write_ptr, write_info_ptr, trans_alpha, num_trans,
                             trans_color);
        }
    }
#endif
#ifdef PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
    {
        png_unknown_chunkp unknowns;
        int num_unknowns = png_get_unknown_chunks(read_ptr, read_info_ptr, &unknowns);
        if (num_unknowns != 0)
        {
            png_set_unknown_chunks(write_ptr, write_info_ptr, unknowns, num_unknowns);
        }
    }
#endif

    youmetest_debug("Writing info struct");

    /* Write the info in two steps so that if we write the 'unknown' chunks here
     * they go to the correct place.
     */
    png_write_info_before_PLTE(write_ptr, write_info_ptr);
    png_write_info(write_ptr, write_info_ptr);

    youmetest_debug("Writing row data");
#ifdef PNG_READ_INTERLACING_SUPPORTED
    num_pass = png_set_interlace_handling(read_ptr);
    if (png_set_interlace_handling(write_ptr) != num_pass)
        png_error(write_ptr, "png_set_interlace_handling: inconsistent num_pass");
#endif

    png_size_t row_buf_size = png_get_rowbytes(read_ptr, read_info_ptr);
    row_buf = (png_bytep)png_malloc(read_ptr, row_buf_size);
    youmetest_debug1("Allocating row buffer...row_buf_size = %d", row_buf_size);
    youmetest_debug1("\t0x%08lx", (unsigned long)row_buf);

    remainder remain = {
            .size = 0
    };
    interlace_param param = {
            .color_type = color_type
    };
    filter f = {
            .param = &param,
            .is_effective = is_effective,
            .get_effective_size = get_effective_size
    };

    int y;
    int write_result;
    size_t pre_remain_size;
    int secret_write = 0;
    for (pass = 0; pass < num_pass; pass++) {
        youmetest_debug1("Writing row data for pass %d", pass);
        for (y = 0; y < height; y++) {
            png_read_row(read_ptr, row_buf, NULL);
            // 将secret隐藏进image中
            if (secret_write < se->size) {
                pre_remain_size = remain.size;
                // 为了让write_secret_to_data在adam7无效行上进行写入的结果始终一样，得保证输入的余数和secret的相关参数一样
                param.pass = num_pass > 1 ? pass : -1;
                write_result = write_secret_to_data(row_buf, 0, row_buf_size,
                                                    se->data, secret_write,
                                                    se->size - secret_write,
                                                    &remain, &f);
                if (row_is_adam7(y, num_pass > 1 ? pass : -1)) {
                    if (write_result >= 0) {
                        secret_write += write_result;
                    }
                } else {
                    remain.size = pre_remain_size;
                }
            }
            png_write_row(write_ptr, row_buf);
        }
    }

#ifdef PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
#  ifdef PNG_READ_UNKNOWN_CHUNKS_SUPPORTED
    png_free_data(read_ptr, read_info_ptr, PNG_FREE_UNKN, -1);
#  endif
#  ifdef PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
    png_free_data(write_ptr, write_info_ptr, PNG_FREE_UNKN, -1);
#  endif
#endif

    youmetest_debug("Reading and writing end_info data");
    png_read_end(read_ptr, end_info_ptr);
#ifdef PNG_TEXT_SUPPORTED
    {
        png_textp text_ptr;
        int num_text;
        if (png_get_text(read_ptr, end_info_ptr, &text_ptr, &num_text) > 0)
        {
            youmetest_debug1("Handling %d iTXt/tEXt/zTXt chunks", num_text);
            png_set_text(write_ptr, write_end_info_ptr, text_ptr, num_text);
        }
    }
#endif
#ifdef PNG_tIME_SUPPORTED
    {
        png_timep mod_time;
        if (png_get_tIME(read_ptr, end_info_ptr, &mod_time) != 0)
        {
            png_set_tIME(write_ptr, write_end_info_ptr, mod_time);
        }
    }
#endif
#ifdef PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
    {
        png_unknown_chunkp unknowns;
        int num_unknowns = png_get_unknown_chunks(read_ptr, end_info_ptr, &unknowns);
        if (num_unknowns != 0)
        {
            png_set_unknown_chunks(write_ptr, write_end_info_ptr, unknowns, num_unknowns);
        }
    }
#endif

    png_write_end(write_ptr, write_end_info_ptr);

#ifdef PNG_EASY_ACCESS_SUPPORTED
    if (verbose != 0)
    {
        png_uint_32 iwidth, iheight;
        iwidth = png_get_image_width(write_ptr, write_info_ptr);
        iheight = png_get_image_height(write_ptr, write_info_ptr);
        fprintf(STDERR, "\n Image width = %lu, height = %lu\n", iwidth, iheight);
    }
#endif

    youmetest_debug("destroying row_buf for read_ptr");
    png_free(read_ptr, row_buf);
    row_buf = NULL;

    youmetest_debug("Destroying data structs");
    youmetest_debug("destroying read_ptr, read_info_ptr, end_info_ptr");
    png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
    youmetest_debug("destroying write_end_info_ptr");
    png_destroy_info_struct(write_ptr, &write_end_info_ptr);
    youmetest_debug("destroying write_ptr, write_info_ptr");
    png_destroy_write_struct(&write_ptr, &write_info_ptr);
    youmetest_debug("Destruction complete.");

    FCLOSE(fpin);
    FCLOSE(fpout);

    return secret_write;
}
