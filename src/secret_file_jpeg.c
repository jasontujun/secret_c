//
// Created by jason on 2016/2/17.
//


#include <stdio.h>
#include <setjmp.h>
#include "jpeglib.h"
#include "secret_file.h"
#include "secret_codec.h"
#include "secret_util.h"


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
#ifdef SAVE_MARKERS_SUPPORTED
    int m;
    /* Save comments except under NONE option */
    jpeg_save_markers(srcinfo, JPEG_COM, 0xFFFF);
    /* Save all types of APPn markers iff ALL option */
    for (m = 0; m < 16; m++)
        jpeg_save_markers(srcinfo, JPEG_APP0 + m, 0xFFFF);
#endif /* SAVE_MARKERS_SUPPORTED */
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
static int wrapper_is_effective(int index, void *param) {
    return 1;
}

/**
 * 获取给定数据内有效数据的总数(用于filter.get_effective_size)
 */
static size_t wrapper_get_effective_size(size_t data_size, void *param) {
    return 0;
}
// ========================== secret_filter的相关参数[end] ========================== //



static int check_jpeg(FILE *file) {
    unsigned char jpeg_signature[3] = {255, 216, 255};
    return check_file_format(file, jpeg_signature, 3);
}

static size_t secret_jpeg_volume(const char *image_file, int has_meta) {
    return 0;
}

static int secret_jpeg_meta(const char *image_file, secret *result) {
    return 0;
}

static int secret_jpeg_dig(const char *se_file, secret *se) {
    return 0;
}

static int secret_jpeg_hide(const char *se_input_file,
                            const char *se_output_file,
                            secret *se) {
    if (!se_input_file || !se_output_file || !se) {
        return -1;
    }

    if (se->file_path == NULL && (se->data == NULL || se->size <= 0)) {
        return -1;
    }

    struct jpeg_decompress_struct srcinfo;
    struct jpeg_compress_struct dstinfo;
    struct my_error_mgr jsrcerr;
    struct my_error_mgr jdsterr;
    jvirt_barray_ptr *coef_arrays;
    FILE * infile;		/* source file */
    FILE * outfile;		/* destination file */

    if ((infile = fopen(se_input_file, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", se_input_file);
        return 0;
    }
    if ((outfile = fopen(se_output_file, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", se_output_file);
        fclose(infile);
        return 0;
    }

    /* Establish the setjmp return context for my_error_exit to use. */
    srcinfo.err = jpeg_std_error(&jsrcerr.pub);
    jsrcerr.pub.error_exit = my_error_exit;
    if (setjmp(jsrcerr.setjmp_buffer)) {
        printf("decompress error!\n");
        jpeg_destroy_decompress(&srcinfo);
        jpeg_destroy_compress(&dstinfo);
        fclose(infile);
        fclose(outfile);
        return 0;
    }
    jpeg_create_decompress(&srcinfo);

    /* Initialize the JPEG compression object with default error handling. */
    dstinfo.err = jpeg_std_error(&jdsterr.pub);
    jdsterr.pub.error_exit = my_error_exit;
    if (setjmp(jdsterr.setjmp_buffer)) {
        printf("compress error!\n");
        jpeg_destroy_decompress(&srcinfo);
        jpeg_destroy_compress(&dstinfo);
        fclose(infile);
        fclose(outfile);
        return 0;
    }
    jpeg_create_compress(&dstinfo);

    /* specify data source (eg, a file) */
    jpeg_stdio_src(&srcinfo, infile);

    /* Enable saving of extra markers that we want to copy */
    jcopy_markers_setup(&srcinfo);

    /* read file parameters with jpeg_read_header() */
    (void) jpeg_read_header(&srcinfo, TRUE);

    /* Read source file as DCT coefficients */
    coef_arrays = jpeg_read_coefficients(&srcinfo);

    /* Initialize destination compression parameters from source values */
    jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

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
            .is_effective = wrapper_is_effective,
            .get_effective_size = wrapper_get_effective_size
    };
    // change dct coefficients
    int total_not_need = 0;
    int total_block = 0;
    int total_volume = 0;
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
                     (JDIMENSION) compptr->v_samp_factor, TRUE);
            for (ri = 0; ri < compptr->v_samp_factor; ri++) {
                buffer_row = buffer_array[ri];
                for (block_num = 0; block_num < compptr->width_in_blocks; block_num++) {
                    for (bi = 0; bi < DCTSIZE2; bi++) {
                        if (buffer_row[block_num][bi] != 0) {
                            total_volume++;
//                            buffer_row[block_num][bi] = to_odd(buffer_row[block_num][bi]);
                        }
                    }
                    total_block++;
                }
            }
        }
    }
    printf("total not need block = %d\n", total_not_need);
    printf("total block = %d\n", total_block);
    printf("total volume = %d bit\n", total_volume);


    /* Specify data destination for compression */
    jpeg_stdio_dest(&dstinfo, outfile);

    /* Start compressor (note no image data is actually written here) */
    jpeg_write_coefficients(&dstinfo, coef_arrays);

    /* Copy to the output file any extra markers that we want to preserve */
    jcopy_markers_execute(&srcinfo, &dstinfo);


    /* Finish compression and release memory */
    jpeg_finish_compress(&dstinfo);
    jpeg_destroy_compress(&dstinfo);
    (void) jpeg_finish_decompress(&srcinfo);
    jpeg_destroy_decompress(&srcinfo);

    fclose(infile);
    fclose(outfile);
    return 1;
}


secret_file_handler jpeg_handler = {
        .secret_file_format = check_jpeg,
        .secret_file_volume = secret_jpeg_volume,
        .secret_file_meta = secret_jpeg_meta,
        .secret_file_dig = secret_jpeg_dig,
        .secret_file_hide = secret_jpeg_hide,
};