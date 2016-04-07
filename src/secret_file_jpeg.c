//
// Created by jason on 2016/2/17.
//


#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include "jpeglib.h"
#include "secret_file.h"
#include "secret_codec.h"
#include "secret_util.h"


#define jpeg_debug(m)        ((void)fprintf(stderr, m "\n"))
#define jpeg_debug1(m,p1)    ((void)fprintf(stderr, m "\n", p1))
#define jpeg_debug2(m,p1,p2) ((void)fprintf(stderr, m "\n", p1, p2))

struct my_error_mgr {
    struct jpeg_error_mgr pub;	/* "public" fields */

    jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

static void my_error_exit (j_common_ptr cinfo) {
    /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
    my_error_ptr myerr = (my_error_ptr) cinfo->err;

    /* Always display the message. */
    /* We could postpone this until after returning, if we chose. */
    (*cinfo->err->output_message) (cinfo);

    /* Return control to the setjmp point */
    longjmp(myerr->setjmp_buffer, 1);
}



static void jcopy_markers_setup (j_decompress_ptr srcinfo) {
    int m;
    /* Save comments except under NONE option */
    jpeg_save_markers(srcinfo, JPEG_COM, 0xFFFF);
    /* Save all types of APPn markers iff ALL option */
    for (m = 0; m < 16; m++)
        jpeg_save_markers(srcinfo, JPEG_APP0 + m, 0xFFFF);
}

static void jcopy_markers_execute (j_decompress_ptr srcinfo, j_compress_ptr dstinfo) {
    jpeg_saved_marker_ptr marker;
    /* In the current implementation, we don't actually need to examine the
     * option flag here; we just copy everything that got saved.
     * But to avoid confusion, we do not output JFIF and Adobe APP14 markers
     * if the encoder library already wrote one.
     */
    for (marker = srcinfo->marker_list; marker != NULL; marker = marker->next) {
        if (dstinfo->write_JFIF_header &&
            marker->marker == JPEG_APP0 &&
            marker->data_length >= 5 &&
            GETJOCTET(marker->data[0]) == 0x4A &&
            GETJOCTET(marker->data[1]) == 0x46 &&
            GETJOCTET(marker->data[2]) == 0x49 &&
            GETJOCTET(marker->data[3]) == 0x46 &&
            GETJOCTET(marker->data[4]) == 0)
            continue;			/* reject duplicate JFIF */
        if (dstinfo->write_Adobe_marker &&
            marker->marker == JPEG_APP0+14 &&
            marker->data_length >= 5 &&
            GETJOCTET(marker->data[0]) == 0x41 &&
            GETJOCTET(marker->data[1]) == 0x64 &&
            GETJOCTET(marker->data[2]) == 0x6F &&
            GETJOCTET(marker->data[3]) == 0x62 &&
            GETJOCTET(marker->data[4]) == 0x65)
            continue;			/* reject duplicate Adobe */
#ifdef NEED_FAR_POINTERS
        /* We could use jpeg_write_marker if the data weren't FAR... */
    {
      unsigned int i;
      jpeg_write_m_header(dstinfo, marker->marker, marker->data_length);
      for (i = 0; i < marker->data_length; i++)
	jpeg_write_m_byte(dstinfo, marker->data[i]);
    }
#else
        jpeg_write_marker(dstinfo, marker->marker,
                          marker->data, marker->data_length);
#endif
    }
}


// ========================== secret_filter的相关参数[start] ========================== //
/**
 * 判断当前索引位置的数据是否有效(用于filter.is_effective)
 */
static int wrapper_is_effective(void *data_p, int index, void *param) {
    JCOEF *data_coef_p = data_p;
    return data_coef_p[index] == 0 ? 0 : 1;// 当前位置为
}
// ========================== secret_filter的相关参数[end] ========================== //


// ========================== 解析meta过程中的回调函数[start] ========================== //
typedef struct {
    int parse_result;// 解析结果：0表示未解析，1表示解析成功，-1表示解析失败
    secret *se;
} meta_parse_meta_param;

// 在解析实际secret的meta部分完成后的回调函数，解析meta的内容。
static void wrapper_meta_parse_meta(void *data, long size, void *param) {
    meta_parse_meta_param *meta_param = param;
    if (secret_get_meta(data, (size_t) size, meta_param->se) > 0) {
        jpeg_debug1("[secret_jpeg_meta]Meta success! data size = %d", meta_param->se->size);
        meta_param->parse_result = 1;
    } else {
        jpeg_debug("[secret_jpeg_meta]Meta error!");
        meta_param->parse_result = -1;
    }
}
// ========================== 解析meta过程中的回调函数[end] ========================== //


// ========================== dig过程中的回调函数[start] ========================== //
typedef struct {
    int parse_result;// 解析结果：0表示未解析，1表示解析成功，-1表示解析失败
    secret *se;
    multi_data_source *ds;
    data_source *ds_data;
    unsigned char **secret_memory;
    size_t *secret_total_size;
} dig_parse_meta_param;

// 在解析实际secret的meta部分完成后的回调函数，解析meta的内容。
static void wrapper_dig_parse_meta(void *data, long size, void *param) {
    dig_parse_meta_param *meta_param = param;
    if (secret_get_meta(data, (size_t) size, meta_param->se) > 0) {
        jpeg_debug1("[secret_jpeg_dig]Meta success! data size = %d", meta_param->se->size);
        meta_param->parse_result = 1;
        // 更新secret的data部分大小和总大小
        if (!meta_param->se->file_path) {
            free(*(meta_param->secret_memory));// 先回收之前申请的内存
            *(meta_param->secret_memory) = malloc(meta_param->se->size);
            change_memory_data_source(*(meta_param->secret_memory), meta_param->ds_data, (long) meta_param->se->size);
        }
        meta_param->ds->resize(meta_param->ds, 1, (long) meta_param->se->size);
        *(meta_param->secret_total_size) = meta_param->se->size + SECRET_META_LENGTH + SECRET_CRC_LENGTH;
    } else {
        jpeg_debug("[secret_jpeg_dig]Meta error!");
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


static int check_jpeg(FILE *file) {
    unsigned char jpeg_signature[3] = {255, 216, 255};
    return check_file_format(file, jpeg_signature, 3);
}

static size_t secret_jpeg_volume(const char *se_file, int has_meta) {
    if (!se_file) {
        return 0;
    }

    struct jpeg_decompress_struct srcinfo;
    struct my_error_mgr jsrcerr;
    jvirt_barray_ptr *coef_arrays;
    FILE * infile;		/* source file */

    if ((infile = fopen(se_file, "rb")) == NULL) {
        jpeg_debug1("[secret_jpeg_dig]Could not find input file %s", se_file);
        return 0;
    }

    // ================== 统一出错处理代码[start] ================== //
    int error_code = 0;
    EXCEPTION: if (error_code != 0) {
        jpeg_destroy_decompress(&srcinfo);
        fclose(infile);
        return 0;
    }
    // ================== 统一出错处理代码[end] ================== //

    srcinfo.err = jpeg_std_error(&jsrcerr.pub);
    jsrcerr.pub.error_exit = my_error_exit;
    if (setjmp(jsrcerr.setjmp_buffer)) {
        jpeg_debug("[secret_jpeg_dig]decompress error!\n");
        error_code = ERROR_COMMON_FILE_READ_FAIL;
        goto EXCEPTION;
    }
    jpeg_create_decompress(&srcinfo);
    srcinfo.mem->max_memory_to_use = 2 * 1024 *1024;

    /* specify data source (eg, a file) */
    jpeg_stdio_src(&srcinfo, infile);

    /* read file parameters with jpeg_read_header() */
    jpeg_read_header(&srcinfo, TRUE);

    /* Read source file as DCT coefficients */
    coef_arrays = jpeg_read_coefficients(&srcinfo);

    // 遍历DCT矩阵
    size_t secret_volume_size = 0;
    jpeg_component_info *compptr;
    JDIMENSION block_num;
    JBLOCKARRAY buffer_array;
    JBLOCKROW buffer_row;
    JDIMENSION height_sample_index;
    int ci, ri, bi;
    for (ci = 0; ci < srcinfo.num_components; ci++) {
        compptr = srcinfo.comp_info + ci;
        for (height_sample_index = 0; height_sample_index < compptr->height_in_blocks;
             height_sample_index += compptr->v_samp_factor) {
            buffer_array = (*srcinfo.mem->access_virt_barray)
                    ((j_common_ptr) (&srcinfo), coef_arrays[ci], height_sample_index,
                     (JDIMENSION) compptr->v_samp_factor, FALSE);
            for (ri = 0; ri < compptr->v_samp_factor; ri++) {
                buffer_row = buffer_array[ri];
                for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
                    for (bi = 0; bi < DCTSIZE2; bi++) {
                        if (buffer_row[block_num][bi] != 0) {
                            secret_volume_size++;
                        }
                    }
                }
            }
        }
    }
    /* Finish compression and release memory */
    jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);
    fclose(infile);

    secret_volume_size = secret_volume_size / 8;
    if (has_meta) {
        secret_volume_size = max(secret_volume_size, SECRET_META_LENGTH + SECRET_CRC_LENGTH)
                             - (SECRET_META_LENGTH + SECRET_CRC_LENGTH);
    }
    return secret_volume_size;
}

static int secret_jpeg_meta(const char *se_file, secret *se) {
    if (!se_file || !se || !se->meta) {
        return 0;
    }

    struct jpeg_decompress_struct srcinfo;
    struct my_error_mgr jsrcerr;
    jvirt_barray_ptr *coef_arrays;
    FILE * infile;		/* source file */
    unsigned char *secret_buf = NULL;// secret提取过程的缓冲区
    unsigned char *meta_buf = NULL;// meta信息的缓冲区
    multi_data_source *ds = NULL;// 多重数据源

    if ((infile = fopen(se_file, "rb")) == NULL) {
        jpeg_debug1("[secret_jpeg_meta]Could not find input file %s", se_file);
        return 0;
    }

    // ================== 统一出错处理代码[start] ================== //
    if (0) {
        EXCEPTION:
        jpeg_destroy_decompress(&srcinfo);
        fclose(infile);
        free(secret_buf);
        free(meta_buf);
        destroy_multi_data_source(ds);
        return 0;
    }
    // ================== 统一出错处理代码[end] ================== //

    /* Establish the setjmp return context for my_error_exit to use. */
    srcinfo.err = jpeg_std_error(&jsrcerr.pub);
    jsrcerr.pub.error_exit = my_error_exit;
    if (setjmp(jsrcerr.setjmp_buffer)) {
        jpeg_debug("[secret_jpeg_meta]decompress error!\n");
        goto EXCEPTION;
    }
    jpeg_create_decompress(&srcinfo);
    srcinfo.mem->max_memory_to_use = 2 * 1024 *1024;

    /* specify data source (eg, a file) */
    jpeg_stdio_src(&srcinfo, infile);

    /* read file parameters with jpeg_read_header() */
    jpeg_read_header(&srcinfo, TRUE);

    /* Read source file as DCT coefficients */
    coef_arrays = jpeg_read_coefficients(&srcinfo);

    // 创建secret缓存区
    const size_t secret_buf_size = DCTSIZE2 / 8 + 2;// 为一行的最大secret数，考虑到余数，必须再加2！
    secret_buf = malloc(secret_buf_size);
    jpeg_debug1("[secret_jpeg_meta]Allocating secret buffer...secret_size = %d", secret_buf_size);
    // 创建多重数据源
    data_source *secret_meta_ds;// 创建secret的meta数据源
    meta_buf = malloc(SECRET_META_LENGTH);
    secret_meta_ds = create_memory_data_source(meta_buf, SECRET_META_LENGTH);
    data_source *source_list[1] = {secret_meta_ds};
    ds = create_multi_data_source(source_list, 1);
    // 设置相关meta信息的处理和crc校验
    meta_parse_meta_param meta_param = {
            .se = se,
            .parse_result = 0
    };
    secret_meta_ds->set_write_full_callback(secret_meta_ds, &meta_param, wrapper_meta_parse_meta);
    // 定义remainder和filter回调
    JCOEF r_buf[7];
    secret_remainder remain = {
            .size = 0,
            .data = r_buf
    };
    secret_filter filter = {
            .param = NULL,
            .is_effective = wrapper_is_effective
    };
    // 遍历DCT矩阵
    int dig_result;
    size_t secret_total_read = 0;
    jpeg_component_info *compptr;
    JDIMENSION block_num;
    JBLOCKARRAY buffer_array;
    JBLOCKROW buffer_row;
    JDIMENSION height_sample_index;
    int ci, ri;
    for (ci = 0; ci < srcinfo.num_components; ci++) {
        compptr = srcinfo.comp_info + ci;
        for (height_sample_index = 0; height_sample_index < compptr->height_in_blocks;
             height_sample_index += compptr->v_samp_factor) {
            buffer_array = (*srcinfo.mem->access_virt_barray)
                    ((j_common_ptr) (&srcinfo), coef_arrays[ci], height_sample_index,
                     (JDIMENSION) compptr->v_samp_factor, FALSE);
            for (ri = 0; ri < compptr->v_samp_factor; ri++) {
                buffer_row = buffer_array[ri];
                for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
                    dig_result = secret_dig(S_SHORT, buffer_row[block_num], 0, DCTSIZE2,
                                            secret_buf, 0, secret_buf_size,
                                            &remain, &filter);
                    if (dig_result > 0) {
                        secret_total_read += dig_result;
                        ds->write(ds, (size_t) dig_result, secret_buf);
                        if (meta_param.parse_result < 0) {
                            // meta信息解析失败
                            jpeg_debug("[secret_jpeg_meta]meta parse error!");
                            goto EXCEPTION;
                        } else if (meta_param.parse_result > 0) {
                            // meta信息解析成功
                            jpeg_debug("[secret_jpeg_meta]meta parse success!");
                            goto FINISH;
                        }
                    }
                }
            }
        }
    }

    FINISH:
    /* Finish compression and release memory */
    jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);
    fclose(infile);
    free(secret_buf);
    free(meta_buf);
    destroy_multi_data_source(ds);
    return 1;
}

static int secret_jpeg_dig(const char *se_file, secret *se) {
    if (!se_file || !se) {
        return ERROR_COMMON_PARAM_NULL;
    }

    struct jpeg_decompress_struct srcinfo;
    struct my_error_mgr jsrcerr;
    jvirt_barray_ptr *coef_arrays;
    FILE * infile;		/* source file */
    FILE * secret_file = NULL;
    unsigned char *secret_memory = NULL;// secret存储的内存空间
    unsigned char *secret_buf = NULL;// secret提取过程的缓冲区
    unsigned char *meta_buf = NULL;// meta信息的缓冲区
    unsigned char *crc_buf = NULL;// crc字节的缓冲区
    multi_data_source *ds = NULL;// 多重数据源

    if ((infile = fopen(se_file, "rb")) == NULL) {
        jpeg_debug1("[secret_jpeg_dig]Could not find input file %s", se_file);
        return ERROR_COMMON_FILE_R_OPEN_FAIL;
    }

    // ================== 统一出错处理代码[start] ================== //
    int error_code = 0;
    EXCEPTION: if (error_code != 0) {
        jpeg_destroy_decompress(&srcinfo);
        fclose(infile);
        if (secret_file) {
            fclose(secret_file);
        }
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

    /* Establish the setjmp return context for my_error_exit to use. */
    srcinfo.err = jpeg_std_error(&jsrcerr.pub);
    jsrcerr.pub.error_exit = my_error_exit;
    if (setjmp(jsrcerr.setjmp_buffer)) {
        jpeg_debug("[secret_jpeg_dig]decompress error!\n");
        error_code = ERROR_COMMON_FILE_READ_FAIL;
        goto EXCEPTION;
    }
    jpeg_create_decompress(&srcinfo);
    srcinfo.mem->max_memory_to_use = 2 * 1024 *1024;

    /* specify data source (eg, a file) */
    jpeg_stdio_src(&srcinfo, infile);

    /* read file parameters with jpeg_read_header() */
    jpeg_read_header(&srcinfo, TRUE);

    /* Read source file as DCT coefficients */
    coef_arrays = jpeg_read_coefficients(&srcinfo);

    // 初步计算secret的字节大小(meta和crc也计算在内，后面会根据meta信息修正这个值)
    size_t secret_total_size;
    if (se->meta) {
        // 带meta格式的提取，忽略外部指定的size字段。先设置一个没用的最小数据量。
        secret_total_size = SECRET_META_LENGTH + SECRET_CRC_LENGTH;
    } else {
        // 不带meta格式的提取，需要外部指定的size字段
        if (se->size <= 0) {
            jpeg_debug1("[secret_jpeg_dig]Need given size: %d!", se->size);
            error_code = ERROR_COMMON_PARAM_NULL;
            goto EXCEPTION;
        }
        secret_total_size = se->size;
    }
    // 创建secret缓存区
    const size_t secret_buf_size = DCTSIZE2 / 8 + 2;// 为一行的最大secret数，考虑到余数，必须再加2！
    secret_buf = malloc(secret_buf_size);
    jpeg_debug1("[secret_jpeg_dig]Allocating secret buffer...secret_size = %d", secret_buf_size);
    // 创建多重数据源
    data_source *secret_meta_ds;// 创建secret的meta数据源
    data_source *secret_data_ds;// 创建secret内容数据源
    data_source *secret_crc_ds;// 创建secret的crc数据源
    dig_parse_meta_param meta_param;
    if (se->file_path) {
        if ((secret_file = fopen(se->file_path, "wb")) == NULL) {
            jpeg_debug1("[secret_jpeg_dig]Could not find secret file %s", se->file_path);
            return ERROR_COMMON_FILE_W_OPEN_FAIL;
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
        dig_parse_meta_param tmp_meta_param = {
                .se = se,
                .parse_result = 0,
                .ds = ds,
                .ds_data = secret_data_ds,
                .secret_memory = &secret_memory,
                .secret_total_size = &secret_total_size
        };
        meta_param = tmp_meta_param;
        secret_meta_ds->set_write_full_callback(secret_meta_ds, &tmp_meta_param, wrapper_dig_parse_meta);
        secret_data_ds->set_write_callback(secret_data_ds, se, wrapper_dig_cal_crc);
    } else {
        data_source *source_list[1] = {secret_data_ds};
        ds = create_multi_data_source(source_list, 1);
    }
    // 定义remainder和filter回调
    JCOEF r_buf[7];
    secret_remainder remain = {
            .size = 0,
            .data = r_buf
    };
    secret_filter filter = {
            .param = NULL,
            .is_effective = wrapper_is_effective
    };
    // 解析DCT矩阵
    int dig_result;
    size_t secret_total_read = 0;
    jpeg_component_info *compptr;
    JDIMENSION block_num;
    JBLOCKARRAY buffer_array;
    JBLOCKROW buffer_row;
    JDIMENSION height_sample_index;
    int ci, ri;
    for (ci = 0; ci < srcinfo.num_components; ci++) {
        compptr = srcinfo.comp_info + ci;
        for (height_sample_index = 0; height_sample_index < compptr->height_in_blocks;
             height_sample_index += compptr->v_samp_factor) {
            buffer_array = (*srcinfo.mem->access_virt_barray)
                    ((j_common_ptr) (&srcinfo), coef_arrays[ci], height_sample_index,
                     (JDIMENSION) compptr->v_samp_factor, FALSE);
            for (ri = 0; ri < compptr->v_samp_factor; ri++) {
                buffer_row = buffer_array[ri];
                for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
                    dig_result = secret_dig(S_SHORT, buffer_row[block_num], 0, DCTSIZE2,
                                            secret_buf, 0, secret_buf_size,
                                            &remain, &filter);
                    if (dig_result > 0) {
                        secret_total_read += dig_result;
                        ds->write(ds, (size_t) dig_result, secret_buf);
                        // meta信息解析失败
                        if (se->meta && meta_param.parse_result < 0) {
                            jpeg_debug("[secret_jpeg_dig]meta parse error!");
                            error_code = ERROR_COMMON_META_PARSE_FAIL;
                            goto EXCEPTION;
                        }
                        // 读取的secret内容已达到指定的长度，退出循环
                        if (secret_total_read >= secret_total_size) {
                            // 验证crc校验码是否一致
                            if (se->meta) {
                                if (secret_check_crc(se->meta->crc, crc_buf)) {
                                    jpeg_debug("[secret_jpeg_dig]CRC error!");
                                    error_code = ERROR_COMMON_CRC_CHECK_FAIL;
                                    goto EXCEPTION;
                                }
                            }
                            jpeg_debug("[secret_jpeg_dig]Parse secret finish!!");
                            goto FINISH;
                        }
                    }
                }
            }
        }
    }

    FINISH:
    /* Finish compression and release memory */
    jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);
    fclose(infile);
    if (secret_file) {
        fclose(secret_file);
    }
    free(secret_buf);
    free(meta_buf);
    free(crc_buf);
    destroy_multi_data_source(ds);
    if (!se->file_path) {
        se->data = secret_memory;
    }
    if (!se->meta) {
        se->size = secret_total_size;
    }

    return secret_total_read;
}

static int secret_jpeg_hide(const char *se_input_file,
                            const char *se_output_file,
                            secret *se) {
    if (!se_input_file || !se_output_file || !se) {
        return ERROR_COMMON_PARAM_NULL;
    }

    if (se->file_path == NULL && (se->data == NULL || se->size <= 0)) {
        return ERROR_COMMON_PARAM_NULL;
    }

    struct jpeg_decompress_struct srcinfo;
    struct jpeg_compress_struct dstinfo;
    struct my_error_mgr jsrcerr;
    struct my_error_mgr jdsterr;
    jvirt_barray_ptr *coef_arrays;
    FILE * infile;		/* source file */
    FILE * outfile;		/* destination file */
    FILE * secret_file = NULL;

    unsigned char *secret_buf = NULL;// secret缓冲区
    unsigned char *meta_buf = NULL;// meta数据区
    unsigned char *crc_buf = NULL;// crc数据区
    multi_data_source *ds = NULL;// 多重数据源

    if ((infile = fopen(se_input_file, "rb")) == NULL) {
        jpeg_debug1("[secret_jpeg_hide]Could not find input file %s", se_input_file);
        return ERROR_COMMON_FILE_R_OPEN_FAIL;
    }
    if ((outfile = fopen(se_output_file, "wb")) == NULL) {
        jpeg_debug1("[secret_jpeg_hide]Could not find output file %s", se_output_file);
        fclose(infile);
        return ERROR_COMMON_FILE_W_OPEN_FAIL;
    }
    if (se->file_path) {
        if ((secret_file = fopen(se->file_path, "rb")) == NULL) {
            jpeg_debug1("[secret_jpeg_hide]Could not find secret file %s", se->file_path);
            fclose(infile);
            fclose(outfile);
            return ERROR_COMMON_FILE_R_OPEN_FAIL;
        }
    }

    // =============== 统一出错处理代码[start] =============== //
    int error_code = 0;
    EXCEPTION:
    if (error_code != 0) {
        jpeg_destroy_decompress(&srcinfo);
        jpeg_destroy_compress(&dstinfo);
        fclose(infile);
        fclose(outfile);
        if (secret_file) {
            fclose(secret_file);
        }
        free(secret_buf);
        free(meta_buf);
        free(crc_buf);
        destroy_multi_data_source(ds);
        //出错的话，删除生成的新的image文件
        remove(se_output_file);
        return error_code;
    }
    // =============== 统一出错处理代码[end] =============== //

    /* Establish the setjmp return context for my_error_exit to use. */
    srcinfo.err = jpeg_std_error(&jsrcerr.pub);
    jsrcerr.pub.error_exit = my_error_exit;
    if (setjmp(jsrcerr.setjmp_buffer)) {
        jpeg_debug("[secret_jpeg_hide]decompress error!\n");
        error_code = ERROR_COMMON_FILE_READ_FAIL;
        goto EXCEPTION;
    }
    jpeg_create_decompress(&srcinfo);
    srcinfo.mem->max_memory_to_use = 2 * 1024 *1024;

    /* Initialize the JPEG compression object with default error handling. */
    dstinfo.err = jpeg_std_error(&jdsterr.pub);
    jdsterr.pub.error_exit = my_error_exit;
    if (setjmp(jdsterr.setjmp_buffer)) {
        jpeg_debug("[secret_jpeg_hide]compress error!\n");
        error_code = ERROR_COMMON_FILE_WRITE_FAIL;
        goto EXCEPTION;
    }
    jpeg_create_compress(&dstinfo);
    dstinfo.mem->max_memory_to_use = 2 * 1024 *1024;

    /* specify data source (eg, a file) */
    jpeg_stdio_src(&srcinfo, infile);

    /* Enable saving of extra markers that we want to copy */
    jcopy_markers_setup(&srcinfo);

    /* read file parameters with jpeg_read_header() */
    jpeg_read_header(&srcinfo, TRUE);

    /* Read source file as DCT coefficients */
    coef_arrays = jpeg_read_coefficients(&srcinfo);

    /* Initialize destination compression parameters from source values */
    jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

    // 如果计算secret的大小(带meta格式，meta和crc也计算在内，加上24+4)
    if (se->file_path) {
        // 如果secret是以文件存储的，先获得文件大小
        fseek(secret_file, 0, SEEK_END);
        se->size = (size_t) ftell(secret_file);
        if (se->size <= 0) {
            jpeg_debug("[secret_jpeg_hide]Secret file size is 0! error!");
            error_code = ERROR_COMMON_FILE_EMPTY;
            goto EXCEPTION;
        }
        rewind(secret_file);
    }
    size_t secret_total_size = se->size + (se->meta ? SECRET_META_LENGTH + SECRET_CRC_LENGTH : 0);
    // 创建secret缓存区
    size_t secret_buf_max = DCTSIZE2 / 8 + 2;// secret缓冲区的大小，考虑到余数，必须再加2！
    secret_buf = malloc(secret_buf_max);
    // 创建meta数据和crc校验码
    if (se->meta) {
        if (secret_create_meta(se, &meta_buf) == -1) {
            jpeg_debug("[secret_jpeg_hide]Meta create error!");
            error_code = ERROR_COMMON_META_CREATE_FAIL;
            goto EXCEPTION;
        }
        se->meta->crc = secret_cal_crc(se);
        if (se->meta->crc == 0) {
            jpeg_debug("[secret_jpeg_hide]CRC calculation error!");
            error_code = ERROR_COMMON_CRC_CAL_FAIL;
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
            jpeg_debug("[secret_jpeg_hide]Secret file read error1!");
            error_code = ERROR_COMMON_FILE_READ_FAIL;
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
    JCOEF r_buf[7];
    secret_remainder remain = {
            .size = 0,
            .data = r_buf
    };
    secret_filter filter = {
            .param = NULL,
            .is_effective = wrapper_is_effective
    };
    // 改变DCT矩阵
    int hide_result = 0;
    long secret_total_write = 0;// 当前已写入的secret字节数
    int secret_buf_size;// secret缓冲区的实际数据大小
    jpeg_component_info *compptr;
    JDIMENSION block_num;
    JBLOCKARRAY buffer_array;
    JBLOCKROW buffer_row;
    JDIMENSION height_sample_index;
    int ci, ri;
    for (ci = 0; ci < srcinfo.num_components; ci++) {
        compptr = srcinfo.comp_info + ci;
        for (height_sample_index = 0; height_sample_index < compptr->height_in_blocks;
             height_sample_index += compptr->v_samp_factor) {
            buffer_array = (*srcinfo.mem->access_virt_barray)
                    ((j_common_ptr) (&srcinfo), coef_arrays[ci], height_sample_index,
                     (JDIMENSION) compptr->v_samp_factor, TRUE);
            for (ri = 0; ri < compptr->v_samp_factor; ri++) {
                buffer_row = buffer_array[ri];
                for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
                    // 如果secret没写入完毕，则继续将secret隐藏进data
                    if (secret_total_write < secret_total_size) {
                        // 从数据源中读入数据进secret缓冲区
                        if (ds->move(ds, secret_total_write) != 0) {
                            jpeg_debug("[secret_jpeg_hide]Secret file read error2!");
                            error_code = ERROR_COMMON_FILE_READ_FAIL;
                            goto EXCEPTION;
                        }
                        secret_buf_size = ds->read(ds, secret_buf_max, secret_buf);
                        if (secret_buf_size == -1) {
                            jpeg_debug("[secret_jpeg_hide]Secret file read error3!");
                            error_code = ERROR_COMMON_FILE_READ_FAIL;
                            goto EXCEPTION;
                        }
                        // 将secret缓冲区中的数据隐藏进image中
                        hide_result = secret_hide(S_SHORT, buffer_row[block_num], 0, DCTSIZE2,
                                                  secret_buf, 0, (size_t) secret_buf_size,
                                                  &remain, &filter);
                        if (hide_result > 0) {
                            secret_total_write += hide_result;
                        }
                    }
                }
            }
        }
    }

    if (secret_total_write < secret_total_size) {
        jpeg_debug1("[secret_jpeg_hide]Not enough to hold all secret! Only hold %ld bytes secret",
                    secret_total_write);
        error_code = ERROR_COMMON_VOLUME_INSUFFICIENT;
        goto EXCEPTION;
    }

    /* Specify data destination for compression */
    jpeg_stdio_dest(&dstinfo, outfile);

    /* Start compressor (note no image data is actually written here) */
    jpeg_write_coefficients(&dstinfo, coef_arrays);

    /* Copy to the output file any extra markers that we want to preserve */
    jcopy_markers_execute(&srcinfo, &dstinfo);

    /* Finish compression and release memory */
    jpeg_finish_compress(&dstinfo);
    jpeg_destroy_compress(&dstinfo);
    jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);
    fclose(infile);
    fclose(outfile);
    if (secret_file) {
        fclose(secret_file);
    }
    free(secret_buf);
    free(meta_buf);
    free(crc_buf);
    destroy_multi_data_source(ds);

    return secret_total_write;
}


secret_file_handler jpeg_handler = {
        .secret_file_format = check_jpeg,
        .secret_file_volume = secret_jpeg_volume,
        .secret_file_meta = secret_jpeg_meta,
        .secret_file_dig = secret_jpeg_dig,
        .secret_file_hide = secret_jpeg_hide,
};