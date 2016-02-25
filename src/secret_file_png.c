//
// Created by jason on 2016/1/6.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png.h"
#include "secret_file.h"
#include "secret_codec.h"
#include "secret_util.h"

#ifndef PNG_DEBUG
#  define PNG_DEBUG 2
#endif

#if PNG_DEBUG > 1
#  define secret_debug(m)        ((void)fprintf(stderr, m "\n"))
#  define secret_debug1(m,p1)    ((void)fprintf(stderr, m "\n", p1))
#  define secret_debug2(m,p1,p2) ((void)fprintf(stderr, m "\n", p1, p2))
#else
#  define secret_debug(m)        ((void)0)
#  define secret_debug1(m,p1)    ((void)0)
#  define secret_debug2(m,p1,p2) ((void)0)
#endif


// ========================== 自定义内存管理函数[start] ========================== //
#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG

typedef struct memory_information {
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


/* Free a pointer.  It is removed from the list at the same time. */
static void PNGCBAPI
png_debug_free(png_structp png_ptr, png_voidp ptr)
{
    if (png_ptr == NULL)
        secret_debug("[png_debug_free]NULL pointer to png_debug_free.");

    if (ptr == 0)
    {
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
                    secret_debug("[png_debug_free]Duplicate free of memory");
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
                secret_debug1("[png_debug_free]Pointer %p not found", ptr);
                break;
            }

            ppinfo = &pinfo->next;
        }
    }

    /* Finally free the data. */
    secret_debug1("[png_debug_free]Freeing %p", ptr);

    if (ptr != NULL)
        free(ptr);
    ptr = NULL;
}

static png_voidp
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

        secret_debug2("[png_debug_free]png_malloc %lu bytes at %p", (unsigned long)size,
                      pinfo->pointer);

        return (png_voidp)(pinfo->pointer);
    }
}
#endif /* USER_MEM && DEBUG */
// ========================== 自定义内存管理函数[end] ========================== //


// ========================== png相关函数[start] ========================== //
char adam7_row_interval[7] = {8, 8, 4, 4, 2, 2, 1};
char adam7_row_offset[7] = {0, 4, 0, 2, 0, 1, 0};
char adam7_col_interval[7] = {8, 8, 8, 4, 4, 2, 2};
char adam7_col_offset[7] = {0, 0, 4, 0, 2, 0, 1};

/**
 * 获取指定颜色类型下，一个颜色像素所占用的字节数。
 * @param color_type 颜色类型(目前只支持0,2,4,6)
 * @return 返回颜色像素所占用的字节数；如果颜色类型不支持或不识别，则返回0。
 */
static size_t get_color_bytes(unsigned char color_type) {
    switch (color_type) {
        case 0:// 灰度图像
            return 1;
        case 2:// 真色彩图像
            return 3;
        case 4:// 带α通道的灰度图像
            return 2;
        case 6:// 带α通道的真色彩图像
            return 4;
        default:
            return 0;// 目前不支持类型3，索引彩色图像，所以认为颜色字节数为0
    }
}

/**
 * 在当前扫描层数，判断一行内的字节索引位置，是否是ada7采样的有效位(默认已经是在有效行内)。
 * @param index 行内的字节索位置
 * @param pass 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @param color_type png图片的颜色类型(0,2,4,6)。
 * @return 如果是有效位，返回1；不是则返回0。
 */
static int col_is_adam7(int index, int pass, unsigned char color_type) {
    if (pass == -1) {
        return 1;
    }
    size_t color_byte = get_color_bytes(color_type);
    if (color_byte == 0) {
        return 0;
    }
    int i = index / color_byte;
    int interval = adam7_row_interval[pass];
    int offset = adam7_row_offset[pass];
    return i % interval == offset;
}

/**
 * 在当前扫描层数，判断某一行是否是ada7采样的有效行。
 * @param index 行索引
 * @param pass 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @param color_type png图片的颜色类型(0,2,4,6)。
 * @return 如果是有效行，返回1；不是则返回0。
 */
static int row_is_adam7(int index, int pass) {
    if (pass == -1) {
        return 1;
    }
    return index % adam7_col_interval[pass] == adam7_col_offset[pass];
}

/**
 * 在当前扫描层数，获取该行中ada7采样的有效位字节数(默认已经是在有效行内)。
 * @param row_byte 当前该行的总字节数
 * @param pass 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @param color_type png图片的颜色类型(0,2,4,6)。
 * @return 如果是有效行，返回1；不是则返回0。
 */
static size_t get_adam7_byte_size(size_t row_byte, int pass, unsigned char color_type) {
    if (pass == -1) {
        return row_byte;
    }
    size_t color_byte = get_color_bytes(color_type);
    if (color_byte == 0) {
        return 0;
    }
    size_t real_size = row_byte / color_byte;
    int interval = adam7_row_interval[pass];
    int offset = adam7_row_offset[pass];
    return (real_size - offset + interval - 1) / interval * color_byte;// 除法向上取整
}
// ========================== png相关函数[end] ========================== //


// ========================== secret_filter的相关参数[start] ========================== //
typedef struct {
    int pass;// 当前扫描的是第几层
    png_byte color_type;// 颜色类型
} interlace_param;

/**
 * 判断当前索引位置的数据是否有效(用于filter.is_effective)
 */
static int wrapper_is_effective(void *data_p, int index, void *param) {
    interlace_param *p = param;
    return col_is_adam7(index, p->pass, p->color_type);
}

// ========================== secret_filter的相关参数[end] ========================== //


// ========================== dig过程中的回调函数[start] ========================== //
typedef struct {
    int parse_result;// 解析结果：0表示未解析，1表示解析成功，-1表示解析失败
    secret *se;
    multi_data_source *ds;
    size_t *secret_total_size;
    size_t max_secret_size;
} parse_meta_param;

// 在解析实际secret的meta部分完成后的回调函数，解析meta的内容。
static void wrapper_dig_parse_meta(void *data, long size, void *param) {
    parse_meta_param *meta_param = param;
    if (secret_get_meta(data, (size_t) size, meta_param->se) > 0) {
        // 取得真正的data数据大小后，验证一下是否超越了容量上限
        if (meta_param->se->size + SECRET_META_LENGTH + SECRET_CRC_LENGTH > meta_param->max_secret_size
            || meta_param->se->size < 0) {
            secret_debug1("[secret_image_dig]Meta error2! meta size incorrect! size=%d", meta_param->se->size);
            meta_param->parse_result = -1;
            return;
        }
        secret_debug1("[secret_image_dig]Meta success! data size = %d", meta_param->se->size);
        meta_param->parse_result = 1;
        // 更新secret的data部分大小和总大小
        meta_param->ds->resize(meta_param->ds, 1, (long) meta_param->se->size);
        *(meta_param->secret_total_size) = meta_param->se->size + SECRET_META_LENGTH + SECRET_CRC_LENGTH;
    } else {
        secret_debug("[secret_image_dig]Meta error!");
        meta_param->parse_result = -1;
    }
}

// 在解析实际secret的data部分过程中的回调函数，计算crc校验码，最后用来比对
static void wrapper_dig_cal_crc(void *data, size_t size, void *param) {
    unsigned char *d = data;
    secret *se = param;
    se->meta->crc = secret_cal_crc2(se->meta->crc, d, size);
}

// ========================== dig过程中的回调函数[start] ========================== //

static int check_png(FILE *file) {
    png_byte png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    return check_file_format(file, png_signature, 4);
}

static size_t secret_png_volume(const char *image_file, int has_meta) {
    if (!image_file) {
        return 0;
    }

    /* "static" prevents setjmp corruption */
    static png_FILE_p fpin;
    png_structp read_ptr;
    png_infop read_info_ptr;

    if ((fpin = fopen(image_file, "rb")) == NULL) {
        secret_debug1("[secret_image_volume]Could not find input file %s", image_file);
        return 0;
    }

#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    read_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL,
                                        NULL, NULL, NULL, png_debug_malloc, png_debug_free);
#else
    read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
#endif
    read_info_ptr = png_create_info_struct(read_ptr);

#ifdef PNG_SETJMP_SUPPORTED
    secret_debug("[secret_image_volume]Setting jmpbuf for read struct");
    if (setjmp(png_jmpbuf(read_ptr))) {
        secret_debug1("[secret_image_volume]%s: libpng read error", image_file);
        png_destroy_read_struct(&read_ptr, &read_info_ptr, NULL);
        fclose(fpin);
        return 0;
    }
#endif

    /* Allow application (pngtest) errors and warnings to pass */
    png_set_benign_errors(read_ptr, 1);
    png_init_io(read_ptr, fpin);
    png_read_info(read_ptr, read_info_ptr);

    png_uint_32 width = png_get_image_width(read_ptr,read_info_ptr);
    png_uint_32 height = png_get_image_height(read_ptr,read_info_ptr);
    png_byte bit_depth = png_get_bit_depth(read_ptr,read_info_ptr);
    png_byte color_type = png_get_color_type(read_ptr,read_info_ptr);
    png_byte interlace_type = png_get_interlace_type(read_ptr,read_info_ptr);
    secret_debug1("[secret_image_volume]image width %d", width);
    secret_debug1("[secret_image_volume]image height %d", height);
    secret_debug1("[secret_image_volume]image bit_depth %d", bit_depth);
    secret_debug1("[secret_image_volume]image color_type %d", color_type);
    secret_debug1("[secret_image_volume]image interlace_type %d", interlace_type);

    size_t max_secret_size = (width * height * get_color_bytes(color_type)) / 8;
    if (has_meta) {
        max_secret_size = max(max_secret_size, SECRET_META_LENGTH) - SECRET_META_LENGTH;
    }

    png_destroy_read_struct(&read_ptr, &read_info_ptr, NULL);
    fclose(fpin);

    return max_secret_size;
}

static int secret_png_meta(const char *image_file, secret *result) {
    return 0;
}

static int secret_png_dig(const char *image_file, secret *se) {
    if (!image_file || !se) {
        return -1;
    }

    /* "static" prevents setjmp corruption */
    static png_FILE_p fpin;
    png_structp read_ptr;
    png_infop read_info_ptr;

    static FILE * secret_file = NULL;// secret存储的文件
    unsigned char *secret_memory = NULL;// secret存储的内存空间

    int num_pass = 1, pass;
    png_bytep row_buf = NULL;// 一行像素的缓冲区
    unsigned char *secret_buf = NULL;// secret提取过程的缓冲区
    unsigned char *meta_buf = NULL;// meta信息的缓冲区
    unsigned char *crc_buf = NULL;// crc字节的缓冲区
    multi_data_source *ds = NULL;// 多重数据源

    if ((fpin = fopen(image_file, "rb")) == NULL) {
        secret_debug1("[secret_image_dig]Could not find input file %s", image_file);
        return -2;
    }

    secret_debug("[secret_image_dig]Allocating read structures");
#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    read_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, NULL,
                                        NULL, NULL, NULL, png_debug_malloc, png_debug_free);
#else
    read_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
#endif
    secret_debug("[secret_image_dig]Allocating read_info structures");
    read_info_ptr = png_create_info_struct(read_ptr);

    // ================== 统一出错处理代码[start] ================== //
    int error_code = 0;
    EXCEPTION: if (error_code != 0) {
        png_free(read_ptr, row_buf);
        row_buf = NULL;
        png_destroy_read_struct(&read_ptr, &read_info_ptr, NULL);
        fclose(fpin);
        fclose(secret_file);
        free(secret_memory);
        free(secret_buf);
        free(meta_buf);
        free(crc_buf);
        destroy_multi_data_source(ds);
        //出错的话，删除生成的secret文件
        if (se->file_path) {
            remove(se->file_path);
        }
        return error_code;
    }
    // ================== 统一出错处理代码[end] ================== //

#ifdef PNG_SETJMP_SUPPORTED
    secret_debug("[secret_image_dig]Setting jmpbuf for read struct");
    if (setjmp(png_jmpbuf(read_ptr))) {
        secret_debug1("[secret_image_dig]%s: libpng read error", image_file);
        error_code = -4;
        goto EXCEPTION;
    }
#endif

    /* Allow application (pngtest) errors and warnings to pass */
    png_set_benign_errors(read_ptr, 1);

    secret_debug("[secret_image_dig]Initializing input streams");
    png_init_io(read_ptr, fpin);

    secret_debug("[secret_image_dig]Reading info struct");
    png_read_info(read_ptr, read_info_ptr);

    png_uint_32 width = png_get_image_width(read_ptr,read_info_ptr);
    png_uint_32 height = png_get_image_height(read_ptr,read_info_ptr);
    png_byte bit_depth = png_get_bit_depth(read_ptr,read_info_ptr);
    png_byte color_type = png_get_color_type(read_ptr,read_info_ptr);
    png_byte interlace_type = png_get_interlace_type(read_ptr,read_info_ptr);
    secret_debug1("[secret_image_dig]image width %d", width);
    secret_debug1("[secret_image_dig]image height %d", height);
    secret_debug1("[secret_image_dig]image bit_depth %d", bit_depth);
    secret_debug1("[secret_image_dig]image color_type %d", color_type);
    secret_debug1("[secret_image_dig]image interlace_type %d", interlace_type);

#ifdef PNG_READ_INTERLACING_SUPPORTED
    num_pass = png_set_interlace_handling(read_ptr);
    if (num_pass != 1 && num_pass != 7) {
        secret_debug1("[secret_image_dig]Image interlace_pass error!! interlace_pass=%d, cannot hide secret!", num_pass);
        error_code = -5;
        goto EXCEPTION;
    }
#endif

    // 计算该图片的secret最大容量
    const size_t max_secret_size = (width * height * get_color_bytes(color_type)) / 8;
    if (max_secret_size == 0) {
        secret_debug1("[secret_image_dig]Image color_type error!! color_type=%d, cannot hide secret!", color_type);
        error_code = -6;
        goto EXCEPTION;
    }
    // 初步计算secret的字节大小(meta和crc也计算在内，后面会根据meta信息修正这个值)
    size_t secret_total_size;
    if (se->meta) {
        // 带meta格式的提取，忽略外部指定的size字段
        const size_t min_secret_size = SECRET_META_LENGTH + SECRET_CRC_LENGTH;// 如果计算meta，secret容量最小值为24+4
        if (max_secret_size < min_secret_size) {
            secret_debug1("[secret_image_dig]Max secret size less then min size: %d!", min_secret_size);
            error_code = -7;
            goto EXCEPTION;
        }
        secret_total_size = max_secret_size;
    } else {
        // 不带meta格式的提取，会参考外部指定的size字段
        if (se->size > 0 && max_secret_size < se->size) {
            secret_debug1("[secret_image_dig]Max secret size less then given size: %d!", se->size);
            error_code = -7;
            goto EXCEPTION;
        }
        secret_total_size = se->size > 0 ? se->size : max_secret_size;
    }
    // 创建row缓冲区
    png_size_t row_buf_size = png_get_rowbytes(read_ptr, read_info_ptr);
    row_buf = (png_bytep)png_malloc(read_ptr, row_buf_size);
    secret_debug1("[secret_image_dig]Allocating row buffer...row_buf_size = %d", row_buf_size);
    // 创建secret缓存区
    const size_t secret_buf_size = row_buf_size / 8 + 2;// 为一行的最大secret数，考虑到余数，必须再加2！
    secret_buf = malloc(secret_buf_size);
    secret_debug1("[secret_image_dig]Allocating secret buffer...secret_size = %d", secret_buf_size);
    // 创建多重数据源
    data_source *secret_meta_ds;// 创建secret的meta数据源
    data_source *secret_data_ds;// 创建secret内容数据源
    data_source *secret_crc_ds;// 创建secret的crc数据源
    parse_meta_param meta_param;
    if (se->file_path) {
        if ((secret_file = fopen(se->file_path, "wb")) == NULL) {
            secret_debug1("[secret_image_dig]Could not find secret file %s", se->file_path);
            return -8;
        }
        secret_data_ds = create_file_data_source(secret_file, (long) secret_total_size);
    } else {
        secret_memory = malloc(secret_total_size);
        secret_data_ds = create_memory_data_source(secret_memory, (long) secret_total_size);
    }
    if (se->meta) {
        meta_buf = malloc(SECRET_META_LENGTH);
        crc_buf = malloc(SECRET_CRC_LENGTH);
        se->meta->crc = secret_cal_crc2(0L, NULL, 0);
        secret_meta_ds = create_memory_data_source(meta_buf, SECRET_META_LENGTH);
        secret_crc_ds = create_memory_data_source(crc_buf, SECRET_CRC_LENGTH);
        data_source *source_list[3] = {secret_meta_ds, secret_data_ds, secret_crc_ds};
        ds = create_multi_data_source(source_list, 3);
        // 设置相关meta信息的处理和crc校验
        parse_meta_param tmp_meta_param = {
                .se = se,
                .parse_result = 0,
                .ds = ds,
                .secret_total_size = &secret_total_size,
                .max_secret_size = max_secret_size
        };
        meta_param = tmp_meta_param;
        secret_meta_ds->set_write_full_callback(secret_meta_ds, &tmp_meta_param, wrapper_dig_parse_meta);
        secret_data_ds->set_write_callback(secret_data_ds, se, wrapper_dig_cal_crc);
    } else {
        data_source *source_list[1] = {secret_data_ds};
        ds = create_multi_data_source(source_list, 1);
    }
    // 定义remainder和filter回调
    interlace_param param = {
            .color_type = color_type
    };
    unsigned char r_buf[7];
    secret_remainder remain = {
            .size = 0,
            .data = r_buf
    };
    secret_filter filter = {
            .param = &param,
            .is_effective = wrapper_is_effective
    };
    // 逐行解析图片
    int y;
    int dig_result;
    size_t secret_total_read = 0;
    for (pass = 0; pass < num_pass; pass++) {
        secret_debug1("[secret_image_dig]Reading row data for pass %d", pass);
        for (y = 0; y < height; y++) {
            png_read_row(read_ptr, row_buf, NULL);
            // 如果不是adam7有效行，直接跳过
            if (!row_is_adam7(y, num_pass > 1 ? pass : -1)) {
                continue;
            }
            // 如果是adam7有效行，提取secret
            param.pass = num_pass > 1 ? pass : -1;
            dig_result = secret_dig(S_U_CHAR, row_buf, 0, row_buf_size,
                                     secret_buf, 0, min(secret_buf_size, secret_total_size - secret_total_read),
                                     &remain, &filter);
            if (dig_result > 0) {
                secret_total_read += dig_result;
                if (ds->write(ds, (size_t) dig_result, secret_buf) < dig_result) {
                    secret_debug1("[secret_image_dig]%s: secret save error!", se->file_path);
                    error_code = -9;
                    goto EXCEPTION;
                }
                // meta信息解析失败
                if (se->meta && meta_param.parse_result < 0) {
                    if (meta_param.parse_result == -1) {
                        error_code = -10;
                        goto EXCEPTION;
                    }
                }
                // 读取的secret内容已达到指定的长度，退出循环
                if (secret_total_read >= secret_total_size) {
                    // 验证crc校验码是否一致
                    if (se->meta) {
                        if (secret_check_crc(se->meta->crc, crc_buf)) {
                            secret_debug("[secret_image_dig]CRC error!");
                            error_code = -11;
                            goto EXCEPTION;
                        }
                    }
                    secret_debug("[secret_image_dig]Parse secret finish!!");
                    goto FINISH;
                }
            }
        }
    }

    FINISH:
    secret_debug("[secret_image_dig]Destroying row_buf for read_ptr");
    png_free(read_ptr, row_buf);
    row_buf = NULL;
    secret_debug("[secret_image_dig]Destroying read_ptr, read_info_ptr");
    png_destroy_read_struct(&read_ptr, &read_info_ptr, NULL);
    fclose(fpin);
    fclose(secret_file);
    free(secret_buf);
    free(meta_buf);
    free(crc_buf);
    if (!se->file_path) {
        se->data = secret_memory;
    }
    if (!se->meta) {
        se->size = secret_total_size;
    }
    return secret_total_size;
}

static int secret_png_hide(const char *image_input_file,
                           const char *image_output_file,
                           secret *se) {
    if (!image_input_file || !image_output_file || !se) {
        return -1;
    }

    if (se->file_path == NULL && (se->data == NULL || se->size <= 0)) {
        return -1;
    }

    /* "static" prevents setjmp corruption */
    static png_FILE_p fpin;
    static png_FILE_p fpout;
    static FILE * secret_file = NULL;
    png_structp read_ptr;
    png_infop read_info_ptr, end_info_ptr;
    png_structp write_ptr;
    png_infop write_info_ptr, write_end_info_ptr;
    int interlace_preserved = 1;

    png_bytep row_buf = NULL;
    int num_pass = 1, pass;
    unsigned char *secret_buf = NULL;// secret缓冲区
    unsigned char *meta_buf = NULL;// meta数据区
    unsigned char *crc_buf = NULL;// crc数据区
    multi_data_source *ds = NULL;// 多重数据源

    if (se->file_path) {
        if ((secret_file = fopen(se->file_path, "rb")) == NULL) {
            secret_debug1("[secret_image_hide]Could not find secret file %s", se->file_path);
            return -2;
        }
    }

    if ((fpin = fopen(image_input_file, "rb")) == NULL) {
        secret_debug1("[secret_image_hide]Could not find input file %s", image_input_file);
        return -3;
    }

    if ((fpout = fopen(image_output_file, "wb")) == NULL) {
        secret_debug1("[secret_image_hide]Could not open output file %s", image_output_file);
        fclose(fpin);
        return -5;
    }

    secret_debug("[secret_image_hide]Allocating read and write structures");
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

    secret_debug("[secret_image_hide]Allocating read_info, write_info and end_info structures");
    read_info_ptr = png_create_info_struct(read_ptr);
    end_info_ptr = png_create_info_struct(read_ptr);
    write_info_ptr = png_create_info_struct(write_ptr);
    write_end_info_ptr = png_create_info_struct(write_ptr);

    // =============== 统一出错处理代码[start] =============== //
    int error_code = 0;
    EXCEPTION:
    if (error_code != 0) {
        png_free(read_ptr, row_buf);
        row_buf = NULL;
        png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
        png_destroy_info_struct(write_ptr, &write_end_info_ptr);
        png_destroy_write_struct(&write_ptr, &write_info_ptr);
        fclose(fpin);
        fclose(fpout);
        fclose(secret_file);
        free(secret_buf);
        free(meta_buf);
        free(crc_buf);
        destroy_multi_data_source(ds);
        //出错的话，删除生成的新的image文件
        remove(image_output_file);
        return error_code;
    }
    // =============== 统一出错处理代码[end] =============== //

#ifdef PNG_SETJMP_SUPPORTED
    if (setjmp(png_jmpbuf(read_ptr))) {
        secret_debug2("[secret_image_hide]%s -> %s: libpng read error", image_input_file, image_output_file);
        error_code = -6;
        goto EXCEPTION;
    }
    if (setjmp(png_jmpbuf(write_ptr))) {
        secret_debug2("[secret_image_hide]%s -> %s: libpng write error", image_input_file, image_output_file);
        error_code = -7;
        goto EXCEPTION;
    }
#endif

    /* Allow application (pngtest) errors and warnings to pass */
    png_set_benign_errors(read_ptr, 1);
    png_set_benign_errors(write_ptr, 1);

    secret_debug("[secret_image_hide]Initializing input and output streams");
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
#ifdef PNG_SAVE_UNKNOWN_CHUNKS_SUPPORTED
    png_set_keep_unknown_chunks(read_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);
#endif
#ifdef PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
    png_set_keep_unknown_chunks(write_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);
#endif
#endif

    secret_debug("[secret_image_hide]Reading info struct");
    png_read_info(read_ptr, read_info_ptr);

    png_uint_32 width = png_get_image_width(read_ptr,read_info_ptr);
    png_uint_32 height = png_get_image_height(read_ptr,read_info_ptr);
    png_byte bit_depth = png_get_bit_depth(read_ptr,read_info_ptr);
    png_byte color_type = png_get_color_type(read_ptr,read_info_ptr);
    png_byte interlace_type = png_get_interlace_type(read_ptr,read_info_ptr);
    secret_debug1("[secret_image_hide]image width %d", width);
    secret_debug1("[secret_image_hide]image height %d", height);
    secret_debug1("[secret_image_hide]image bit_depth %d", bit_depth);
    secret_debug1("[secret_image_hide]image color_type %d", color_type);
    secret_debug1("[secret_image_hide]image interlace_type %d", interlace_type);

#ifdef PNG_READ_INTERLACING_SUPPORTED
    num_pass = png_set_interlace_handling(read_ptr);
    if (num_pass != 1 && num_pass != 7) {
        secret_debug1("[secret_image_hide]Image interlace_pass error!! interlace_pass=%d, cannot hide secret!", num_pass);
        error_code = -8;
        goto EXCEPTION;
    }
#endif

    // 计算该图片的secret最大容量
    size_t max_secret_size = (width * height * get_color_bytes(color_type)) / 8;
    if (max_secret_size == 0) {
        secret_debug1("[secret_image_hide]Image color_type error!! color_type=%d, cannot hide secret!", color_type);
        error_code = -9;
        goto EXCEPTION;
    }
    size_t min_secret_size = 0;
    if (se->meta) {// 如果计算meta，secret容量最小值为24+4
        min_secret_size = SECRET_META_LENGTH + SECRET_CRC_LENGTH;
        if (max_secret_size < min_secret_size) {
            secret_debug1("[secret_image_hide]Max secret size less then min size: %d!", min_secret_size);
            error_code = -10;
            goto EXCEPTION;
        }
    }
    // 计算secret的字节大小(如果带meta格式的，meta和crc也计算在内)
    if (se->file_path) {
        // 如果secret是以文件存储的，先获得文件大小
        fseek(secret_file, 0, SEEK_END);
        se->size = (size_t) ftell(secret_file);
        if (se->size <= 0) {
            secret_debug("[secret_image_hide]Secret file size is 0! error!");
            error_code = -11;
            goto EXCEPTION;
        }
        rewind(secret_file);
    }
    size_t secret_total_size = se->size + min_secret_size;// 如果是带meta格式，要加上meta和crc的大小
    // 判断图片是能容纳的下所有secret信息
    if (max_secret_size < secret_total_size) {
        secret_debug2("[secret_image_hide]Max volume less then secret size! max_volume=%d, secret_size=%d!",
                      max_secret_size, secret_total_size);
        error_code = -10;
        goto EXCEPTION;
    }

    secret_debug("[secret_image_hide]Transferring info struct");
    {
        png_uint_32 width2, height2;
        int bit_depth2, color_type2, interlace_type2, compression_type2, filter_type2;
        if (png_get_IHDR(read_ptr, read_info_ptr, &width2, &height2, &bit_depth2,
                         &color_type2, &interlace_type2, &compression_type2, &filter_type2) != 0) {
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
                               &red_x, &red_y, &green_x, &green_y, &blue_x, &blue_y) != 0) {
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
         &red_y, &green_x, &green_y, &blue_x, &blue_y) != 0) {
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
        if (png_get_iCCP(read_ptr, read_info_ptr, &name, &compression_type2, &profile, &proflen) != 0) {
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
        if (png_get_bKGD(read_ptr, read_info_ptr, &background) != 0) {
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
        if (png_get_oFFs(read_ptr, read_info_ptr, &offset_x, &offset_y, &unit_type) != 0) {
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
                         &nparams, &units, &params) != 0) {
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
        if (png_get_sCAL(read_ptr, read_info_ptr, &unit, &scal_width, &scal_height) != 0) {
            png_set_sCAL(write_ptr, write_info_ptr, unit, scal_width, scal_height);
        }
    }
#else
    #ifdef PNG_FIXED_POINT_SUPPORTED
   {
      int unit;
      png_charp scal_width, scal_height;
      if (png_get_sCAL_s(read_ptr, read_info_ptr, &unit, &scal_width, &scal_height) != 0) {
         png_set_sCAL_s(write_ptr, write_info_ptr, unit, scal_width, scal_height);
      }
   }
#endif
#endif
#endif
#ifdef PNG_TEXT_SUPPORTED
    {
        png_textp text_ptr;
        int num_text;
        if (png_get_text(read_ptr, read_info_ptr, &text_ptr, &num_text) > 0) {
            secret_debug1("[secret_image_hide]Handling %d iTXt/tEXt/zTXt chunks", num_text);
            png_set_text(write_ptr, write_info_ptr, text_ptr, num_text);
        }
    }
#endif
#ifdef PNG_tIME_SUPPORTED
    {
        png_timep mod_time;
        if (png_get_tIME(read_ptr, read_info_ptr, &mod_time) != 0) {
            png_set_tIME(write_ptr, write_info_ptr, mod_time);
        }
    }
#endif
#ifdef PNG_tRNS_SUPPORTED
    {
        png_bytep trans_alpha;
        int num_trans;
        png_color_16p trans_color;
        if (png_get_tRNS(read_ptr, read_info_ptr, &trans_alpha, &num_trans, &trans_color) != 0) {
            int sample_max = (1 << bit_depth);
            /* libpng doesn't reject a tRNS chunk with out-of-range samples */
            if (!((color_type == PNG_COLOR_TYPE_GRAY &&
                   (int)trans_color->gray > sample_max) ||
                  (color_type == PNG_COLOR_TYPE_RGB &&
                   ((int)trans_color->red > sample_max ||
                    (int)trans_color->green > sample_max ||
                    (int)trans_color->blue > sample_max))))
                png_set_tRNS(write_ptr, write_info_ptr, trans_alpha, num_trans, trans_color);
        }
    }
#endif
#ifdef PNG_WRITE_UNKNOWN_CHUNKS_SUPPORTED
    {
        png_unknown_chunkp unknowns;
        int num_unknowns = png_get_unknown_chunks(read_ptr, read_info_ptr, &unknowns);
        if (num_unknowns != 0) {
            png_set_unknown_chunks(write_ptr, write_info_ptr, unknowns, num_unknowns);
        }
    }
#endif

    secret_debug("[secret_image_hide]Writing info struct");

    /* Write the info in two steps so that if we write the 'unknown' chunks here
     * they go to the correct place.
     */
    png_write_info_before_PLTE(write_ptr, write_info_ptr);
    png_write_info(write_ptr, write_info_ptr);

    secret_debug("[secret_image_hide]Writing row data");
#ifdef PNG_READ_INTERLACING_SUPPORTED
    num_pass = png_set_interlace_handling(read_ptr);
    if (png_set_interlace_handling(write_ptr) != num_pass)
        png_error(write_ptr, "png_set_interlace_handling: inconsistent num_pass");
#endif

    // 创建row缓冲区
    png_size_t row_buf_size = png_get_rowbytes(read_ptr, read_info_ptr);
    row_buf = (png_bytep)png_malloc(read_ptr, row_buf_size);
    secret_debug1("[secret_image_hide]Allocating row buffer...row_buf_size = %d", row_buf_size);
    // 创建secret缓存区
    size_t secret_buf_max = row_buf_size / 8 + 2;// secret缓冲区的大小，考虑到余数，必须再加2！
    secret_buf = malloc(secret_buf_max);
    // 创建meta数据和crc校验码
    if (se->meta) {
        if (secret_create_meta(se, &meta_buf) == -1) {
            secret_debug("[secret_image_hide]Meta create error!");
            error_code = -13;
            goto EXCEPTION;
        }
        se->meta->crc = secret_cal_crc(se);
        if (se->meta->crc == 0) {
            secret_debug("[secret_image_hide]CRC calculation error!");
            error_code = -14;
            goto EXCEPTION;
        }
    }
    // 创建多重数据源
    data_source *secret_meta_ds;// 创建secret的meta数据源
    data_source *secret_data_ds;// 创建secret内容数据源
    data_source *secret_crc_ds;// 创建secret的crc数据源
    if (se->file_path) {
        rewind(secret_file);
        secret_data_ds = create_file_data_source(secret_file, -1);
        if (!secret_data_ds) {
            secret_debug("[secret_image_hide]Secret file read error1!");
            error_code = -12;
            goto EXCEPTION;
        }
    } else {
        secret_data_ds = create_memory_data_source(se->data, (long) se->size);
    }
    if (se->meta) {
        secret_meta_ds = create_memory_data_source(meta_buf, SECRET_META_LENGTH);
        crc_buf = malloc(SECRET_CRC_LENGTH);
        ulong_to_byte(se->meta->crc, crc_buf, SECRET_CRC_LENGTH);
        secret_crc_ds = create_memory_data_source(crc_buf, SECRET_CRC_LENGTH);
        data_source *source_list[3] = {secret_meta_ds, secret_data_ds, secret_crc_ds};
        ds = create_multi_data_source(source_list, 3);
    } else {
        data_source *source_list[1] = {secret_data_ds};
        ds = create_multi_data_source(source_list, 1);
    }
    // 定义remainder和filter回调
    interlace_param param = {
            .color_type = color_type
    };
    unsigned char r_buf[7];
    secret_remainder remain = {
            .size = 0,
            .data = r_buf
    };
    secret_filter filter = {
            .param = &param,
            .is_effective = wrapper_is_effective
    };
    // 逐行写入图片
    int y;
    int hide_result = 0;
    long secret_total_write = 0;// 当前已写入的secret字节数
    int secret_buf_size;// secret缓冲区的实际数据大小
    for (pass = 0; pass < num_pass; pass++) {
        secret_debug1("[secret_image_hide]Writing row data for pass %d", pass);
        for (y = 0; y < height; y++) {
            png_read_row(read_ptr, row_buf, NULL);
            // 如果secret没写入完毕，且是adam7有效行
            if (secret_total_write < secret_total_size && row_is_adam7(y, num_pass > 1 ? pass : -1)) {
                // 从数据源中读入数据进secret缓冲区
                if (ds->move(ds, secret_total_write) != 0) {
                    secret_debug("[secret_image_hide]Secret file read error2!");
                    error_code = -12;
                    goto EXCEPTION;
                }
                secret_buf_size = ds->read(ds, secret_buf_max, secret_buf);
                if (secret_buf_size == -1) {
                    secret_debug("[secret_image_hide]Secret file read error3!");
                    error_code = -12;
                    goto EXCEPTION;
                }
                // 将secret缓冲区中的数据隐藏进image中
                param.pass = num_pass > 1 ? pass : -1;
                hide_result = secret_hide(S_U_CHAR, row_buf, 0, row_buf_size,
                                          secret_buf, 0, (size_t) secret_buf_size,
                                          &remain, &filter);
                if (hide_result > 0) {
                    secret_total_write += hide_result;
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

    secret_debug("[secret_image_hide]Reading and writing end_info data");
    png_read_end(read_ptr, end_info_ptr);
#ifdef PNG_TEXT_SUPPORTED
    {
        png_textp text_ptr;
        int num_text;
        if (png_get_text(read_ptr, end_info_ptr, &text_ptr, &num_text) > 0) {
            secret_debug1("[secret_image_hide]Handling %d iTXt/tEXt/zTXt chunks", num_text);
            png_set_text(write_ptr, write_end_info_ptr, text_ptr, num_text);
        }
    }
#endif
#ifdef PNG_tIME_SUPPORTED
    {
        png_timep mod_time;
        if (png_get_tIME(read_ptr, end_info_ptr, &mod_time) != 0) {
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

    secret_debug("[secret_image_hide]Destroying row_buf for read_ptr");
    png_free(read_ptr, row_buf);
    row_buf = NULL;
    secret_debug("[secret_image_hide]Destroying read_ptr, read_info_ptr, end_info_ptr");
    png_destroy_read_struct(&read_ptr, &read_info_ptr, &end_info_ptr);
    secret_debug("[secret_image_hide]Destroying write_end_info_ptr");
    png_destroy_info_struct(write_ptr, &write_end_info_ptr);
    secret_debug("[secret_image_hide]Destroying write_ptr, write_info_ptr");
    png_destroy_write_struct(&write_ptr, &write_info_ptr);
    secret_debug("[secret_image_hide]Destruction complete.");
    fclose(fpin);
    fclose(fpout);
    fclose(secret_file);
    free(secret_buf);
    free(meta_buf);
    free(crc_buf);
    destroy_multi_data_source(ds);


#if defined(PNG_USER_MEM_SUPPORTED) && PNG_DEBUG
    secret_debug1("[secret_image_hide] Current memory allocation: %10d bytes", current_allocation);
    secret_debug1("[secret_image_hide] Maximum memory allocation: %10d bytes", maximum_allocation);
    secret_debug1("[secret_image_hide] Total   memory allocation: %10d bytes", total_allocation);
    secret_debug1("[secret_image_hide]     Number of allocations: %10d", num_allocations);
#endif

    return secret_total_write;
}

secret_file_handler png_handler = {
        .secret_file_format = check_png,
        .secret_file_volume = secret_png_volume,
        .secret_file_meta = secret_png_meta,
        .secret_file_dig = secret_png_dig,
        .secret_file_hide = secret_png_hide,
};