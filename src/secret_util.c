//
// Created by jason on 2016/1/7.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png.h"
#include "secret_util.h"

#define PNG_BYTES_TO_CHECK 4

int check_png(char *file_path) {
    FILE *file = fopen(file_path, "rb");
    int result = check_png2(file);
    if (file)
        fclose(file);
    return result;
}

int check_png2(FILE *file) {
    png_byte buf[PNG_BYTES_TO_CHECK];
    if (file == NULL)
        return 0;
    rewind(file);
    if (fread(buf, 1, PNG_BYTES_TO_CHECK, file) != PNG_BYTES_TO_CHECK) {
        rewind(file);
        return 0;
    }
    // 对比文件前4个magic字节
    int result = !png_sig_cmp(buf, 0, PNG_BYTES_TO_CHECK);
    // 将文件指针指回文件开头，不影响后续libpng对文件的操作
    rewind(file);
    return result;
}

char adam7_row_interval[7] = {8, 8, 4, 4, 2, 2, 1};
char adam7_row_offset[7] = {0, 4, 0, 2, 0, 1, 0};
char adam7_col_interval[7] = {8, 8, 8, 4, 4, 2, 2};
char adam7_col_offset[7] = {0, 0, 4, 0, 2, 0, 1};

size_t get_color_bytes(unsigned char color_type) {
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

int col_is_adam7(int index, int pass, unsigned char color_type) {
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

int row_is_adam7(int index, int pass) {
    if (pass == -1) {
        return 1;
    }
    return index % adam7_col_interval[pass] == adam7_col_offset[pass];
}

size_t get_adam7_byte_size(size_t row_byte, int pass, unsigned char color_type) {
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



static int mask[4] = {0xFF, 0xFF00, 0xFF0000, 0xFF000000};

size_t byte_to_sizet(unsigned char* bytes, size_t length) {
    if (length <= 0) {
        return 0;
    }
    if (length > 4) {
        length = 4;
    }
    size_t result = 0;
    int i;
    for (i= 0; i < length; i++) {
        result |= ((bytes[length - 1 - i] << (8 * i)) & mask[i]);
    }
    return result;
}

int sizet_to_byte(size_t integer, unsigned char* bytes, size_t length) {
    if (length <= 0) {
        return 0;
    }
    if (length > 4) {
        length = 4;
    }
    int i;
    for (i= 0; i < length; i++) {
        bytes[length - 1 - i] = (unsigned char) ((integer & mask[i]) >> (8 * i));
    }
    return 1;
}

int ulong_to_byte(unsigned long integer, unsigned char* bytes, size_t length) {
    if (length <= 0) {
        return 0;
    }
    if (length > 4) {
        length = 4;
    }
    int i;
    for (i= 0; i < length; i++) {
        bytes[length - 1 - i] = (unsigned char) ((integer & mask[i]) >> (8 * i));
    }
    return 1;
}


// =============== data_source相关代码[start] =============== //

typedef struct _file_data {
    FILE *file;
    long init_pos;// 文件的初始指针位置
    void (*read_callback)(void *, size_t, void *);// 读操作的回调函数指针
    void *read_callback_param;// 读操作的回调函数的参数
    void (*write_callback)(void *, size_t, void *);// 写操作的回调函数指针
    void *write_callback_param;// 写操作的回调函数的参数
    void (*read_full_callback)(void *, long, void *);// 读满的回调函数指针
    void *read_full_param;// 读满的回调函数的参数
    void (*write_full_callback)(void *, long, void *);// 写满的回调函数指针
    void *write_full_param;// 写满的回调函数的参数
} file_data;
static int file_read_wrapper(data_source *source, size_t size, void * buf) {
    file_data *fdata = source->data;
    // 读文件不能超出指定大小
    long cur_pos = source->pos(source);
    if (cur_pos < 0) {
        return -1;
    }
    size = min((size_t)(source->size - cur_pos), size);
    if (size == 0) {
        return 0;
    }
    size_t result = fread(buf, 1, size, fdata->file);
    if (ferror(fdata->file)) {
        return -1;
    } else {
        if (fdata->read_callback) {
            fdata->read_callback(buf, result, fdata->read_callback_param);
        }
        if (fdata->read_full_callback && (result + cur_pos >= source->size)) {
            fdata->read_full_callback(fdata->file, source->size, fdata->read_full_param);
        }
        return result;
    }
}
static int file_write_wrapper(data_source *source, size_t size, void * buf) {
    file_data *fdata = source->data;
    // 写文件不能超出指定大小
    long cur_pos = source->pos(source);
    if (cur_pos < 0) {
        return -1;
    }
    size = min((size_t)(source->size - cur_pos), size);
    if (size == 0) {
        return 0;
    }
    size_t result = fwrite(buf, 1, size, fdata->file);
    if (ferror(fdata->file)) {
        return -1;
    } else {
        if (fdata->write_callback) {
            fdata->write_callback(buf, result, fdata->write_callback_param);
        }
        if (fdata->write_full_callback && (result + cur_pos >= source->size)) {
            fdata->write_full_callback(fdata->file, source->size, fdata->write_full_param);
        }
        return result;
    }
}
static int file_move_wrapper(data_source *source, long new_pos) {
    file_data *fdata = source->data;
    new_pos = min(max(0, new_pos), source->size);// 文件指针只能在0~size间移动
    return fseek(fdata->file, fdata->init_pos + new_pos, SEEK_SET);// 实际移动时加上起始偏移位置
}
static long file_pos_wrapper(data_source *source) {
    file_data *fdata = source->data;
    return ftell(fdata->file) - fdata->init_pos;
}
static void file_set_read_callback_wrapper(data_source *source, void *param,
                                           void (*callback)(void *, size_t, void *)) {
    file_data *fdata = source->data;
    fdata->read_callback_param = param;
    fdata->read_callback = callback;
}
static void file_set_write_callback_wrapper(data_source *source, void *param,
                                            void (*callback)(void *, size_t, void *)) {
    file_data *fdata = source->data;
    fdata->write_callback_param = param;
    fdata->write_callback = callback;
}
static void file_set_read_full_callback_wrapper(data_source *source, void *param,
                                                void (*callback)(void *, long, void *)) {
    file_data *fdata = source->data;
    fdata->read_full_param = param;
    fdata->read_full_callback = callback;
}
static void file_set_write_full_callback_wrapper(data_source *source, void *param,
                                                 void (*callback)(void *, long, void *)) {
    file_data *fdata = source->data;
    fdata->write_full_param = param;
    fdata->write_full_callback = callback;
}


typedef struct _mem_data {
    unsigned char *mem;
    long pos;
    void (*read_callback)(void *, size_t, void *);// 读操作的回调函数指针
    void *read_callback_param;// 读操作的回调函数的参数
    void (*write_callback)(void *, size_t, void *);// 写操作的回调函数指针
    void *write_callback_param;// 写操作的回调函数的参数
    void (*read_full_callback)(void *, long, void *);// 读满的回调函数指针
    void *read_full_param;// 读满的回调函数的参数
    void (*write_full_callback)(void *, long, void *);// 写满的回调函数指针
    void *write_full_param;// 写满的回调函数的参数
} mem_data;
static int mem_read_wrapper(data_source *source, size_t size, void * buf) {
    mem_data *mem = source->data;
    size_t read_size = min((size_t)(source->size - mem->pos), size);
    if (read_size > 0) {
        memcpy(buf, mem->mem + mem->pos, read_size);
        mem->pos += read_size;
        if (mem->read_callback) {
            mem->read_callback(buf, read_size, mem->read_callback_param);
        }
        if (mem->read_full_callback && (mem->pos >= source->size)) {
            mem->read_full_callback(mem->mem, source->size, mem->read_full_param);
        }
    }
    return read_size;
}
static int mem_write_wrapper(data_source *source, size_t size, void * buf) {
    mem_data *mem = source->data;
    size_t write_size = min((size_t)(source->size - mem->pos), size);
    if (write_size > 0) {
        memcpy(mem->mem + mem->pos, buf, write_size);
        mem->pos += write_size;
        if (mem->write_callback) {
            mem->write_callback(buf, write_size, mem->write_callback_param);
        }
        if (mem->write_full_callback && (mem->pos >= source->size)) {
            mem->write_full_callback(mem->mem, source->size, mem->write_full_param);
        }
    }
    return write_size;
}
static int mem_move_wrapper(data_source *source, long new_pos) {
    mem_data *mem = source->data;
    mem->pos = min(max(0, new_pos), source->size);
    return 0;
}
static long mem_pos_wrapper(data_source *source) {
    mem_data *mem = source->data;
    return mem->pos;
}
static void mem_set_read_callback_wrapper(data_source *source, void *param,
                                          void (*callback)(void *, size_t, void *)) {
    mem_data *mem = source->data;
    mem->read_callback_param = param;
    mem->read_callback = callback;
}
static void mem_set_write_callback_wrapper(data_source *source, void *param,
                                           void (*callback)(void *, size_t, void *)) {
    mem_data *mem = source->data;
    mem->write_callback_param = param;
    mem->write_callback = callback;
}
static void mem_set_read_full_callback_wrapper(data_source *source, void *param,
                                        void (*callback)(void *, long, void *)) {
    mem_data *mem = source->data;
    mem->read_full_param = param;
    mem->read_full_callback = callback;
}
static void mem_set_write_full_callback_wrapper(data_source *source, void *param,
                                         void (*callback)(void *, long, void *)) {
    mem_data *mem = source->data;
    mem->write_full_param = param;
    mem->write_full_callback = callback;
}


typedef struct _multi_data_source_member {
    size_t source_count;
    data_source **source_list;
    long total_size;
    long cur_pos;
    int cur_source_index;
} multi_data_source_member;
static int multi_read_wrapper(multi_data_source *multi, size_t size, void * buf) {
    if (!multi || !multi->member) {
        return -1;
    }
    multi_data_source_member *member = multi->member;
    size_t read_size = min((size_t)(member->total_size - member->cur_pos), size);
    size_t cur_read_size = 0;
    data_source *cur_source;
    int read_result;
    while (cur_read_size < read_size) {
        cur_source = multi->get_source(multi, member->cur_source_index);
        if (!cur_source) {// 当前没有可用的数据源，不应该发生！
            return 0;
        }
        read_result = cur_source->read(cur_source,
                                       read_size - cur_read_size,
                                       buf + cur_read_size);
        if (read_result < 0) {// 读操作出错了
            multi->move(multi, member->cur_pos);// 恢复原样
            return -1;
        }
        cur_read_size += read_result;
        if (cur_source->pos(cur_source) == cur_source->size) {
            member->cur_source_index += 1;
        }
    }
    member->cur_pos += read_size;
    return read_size;
}
static int multi_write_wrapper(multi_data_source *multi, size_t size, void * buf) {
    if (!multi || !multi->member) {
        return -1;
    }
    multi_data_source_member *member = multi->member;
    size_t write_size = min((size_t)(member->total_size - member->cur_pos), size);
    size_t cur_write_size = 0;
    data_source *cur_source;
    int write_result;
    while (cur_write_size < write_size) {
        cur_source = multi->get_source(multi, member->cur_source_index);
        if (!cur_source) {// 当前没有可用的数据源，不应该发生！
            return 0;
        }
        write_result = cur_source->write(cur_source,
                                         write_size - cur_write_size,
                                         buf + cur_write_size);
        if (write_result < 0) {// 写操作出错了
            multi->move(multi, member->cur_pos);// 恢复原样
            return -1;
        }
        cur_write_size += write_result;
        if (cur_source->pos(cur_source) == cur_source->size) {
            member->cur_source_index += 1;
        }
    }
    member->cur_pos += write_size;
    return write_size;
}
static int multi_move_wrapper(multi_data_source *multi, long new_pos) {
    if (!multi || !multi->member) {
        return -1;
    }
    multi_data_source_member *member = multi->member;
    new_pos = min(max(0, new_pos), member->total_size);
    // 更新每个子数据源的指针
    long source_left = 0;
    long source_right = 0;
    int i;
    data_source *cur_source;
    long cur_source_pos;
    for (i = 0; i < member->source_count; i++) {
        cur_source = member->source_list[i];
        source_left = source_right;
        source_right += cur_source->size;
        if (new_pos >= source_right) {
            cur_source_pos = cur_source->size;
        } else if (new_pos < source_left) {
            cur_source_pos = 0;
        } else {
            cur_source_pos = new_pos - source_left;
            member->cur_source_index = i;
        }
        if (cur_source->move(cur_source, cur_source_pos) != 0) {
            return -1;
        }
    }
    // 更新本身的指针
    member->cur_pos = new_pos;
    if (new_pos == member->total_size) {
        // 如果指针移动到最后，则索引指向最后一个数据源的后一个位置(越界)。
        member->cur_source_index = member->source_count;
    }
    return 0;
}
static data_source * multi_get_source_wrapper(multi_data_source *multi, int index) {
    if (!multi || !multi->member) {
        return NULL;
    }
    multi_data_source_member *member = multi->member;
    if (!member->source_list) {
        return NULL;
    }
    if (0 <= index && index < member->source_count) {
        return member->source_list[index];
    } else {
        return NULL;
    }
}
static size_t multi_get_source_count_wrapper(multi_data_source *multi) {
    if (!multi || !multi->member) {
        return 0;
    }
    multi_data_source_member *member = multi->member;
    return member->source_count;
}
static long multi_size_wrapper(multi_data_source *multi) {
    if (!multi || !multi->member) {
        return 0L;
    }
    multi_data_source_member *member = multi->member;
    return member->total_size;
}
static int multi_resize_wrapper(multi_data_source *multi, int index, long new_size) {
    if (!multi || !multi->member || new_size < 0) {
        return -1;
    }
    // 不能修改当前指针位置之前的数据源大小
    multi_data_source_member *member = multi->member;
    if (member->cur_source_index > index) {
        return -2;
    }
    data_source *source = multi->get_source(multi, index);
    if (!source) {
        return -3;
    }
    // 增大好办,减小可能要改变子数据源的指针位置
    long delta = new_size - source->size;
    if (delta < 0) {
        long pos = source->pos(source);
        if (pos > new_size) {
            // 修改子数据源的指针
            source->move(source, new_size);
            // 修改本身的指针
            if (member->cur_source_index == index) {
                member->cur_pos += delta;
            }
        }
    }
    source->size = new_size;
    // 重新计算总大小
    int i;
    for (i = 0; i < member->source_count; i++) {
        member->total_size += (member->source_list[i])->size;
    }
    return 0;
}


data_source * create_file_data_source(FILE *file, long size) {
    if (!file || ferror(file)) {
        return NULL;
    }
    data_source *source = malloc(sizeof(data_source));

    file_data *data = malloc(sizeof(file_data));
    data->file = file;
    data->init_pos = ftell(file);
    data->read_callback = NULL;
    data->read_callback_param = NULL;
    data->write_callback = NULL;
    data->write_callback_param = NULL;
    data->read_full_callback = NULL;
    data->read_full_param = NULL;
    data->write_full_callback = NULL;
    data->write_full_param = NULL;
    if (data->init_pos < 0) {
        free(source);
        return NULL;
    }

    source->data = data;
    if (size >= 0) {
        source->size = size;
    } else {
        fseek(file, 0, SEEK_END);
        source->size = ftell(file) - data->init_pos;
        if (source->size < 0) {
            free(source);
            return NULL;
        }
        fseek(file, data->init_pos, SEEK_SET);
    }
    source->read = file_read_wrapper;
    source->write = file_write_wrapper;
    source->move = file_move_wrapper;
    source->pos = file_pos_wrapper;
    source->set_read_callback = file_set_read_callback_wrapper;
    source->set_write_callback = file_set_write_callback_wrapper;
    source->set_read_full_callback = file_set_read_full_callback_wrapper;
    source->set_write_full_callback = file_set_write_full_callback_wrapper;
    return source;
}

data_source * create_memory_data_source(unsigned char *mem, long size) {
    if (!mem || size < 0) {
        return NULL;
    }
    data_source *source = malloc(sizeof(data_source));

    mem_data *data = malloc(sizeof(mem_data));
    data->mem = mem;
    data->pos = 0;
    data->read_callback = NULL;
    data->read_callback_param = NULL;
    data->write_callback = NULL;
    data->write_callback_param = NULL;
    data->read_full_callback = NULL;
    data->read_full_param = NULL;
    data->write_full_callback = NULL;
    data->write_full_param = NULL;

    source->data = data;
    source->size = size;
    source->read = mem_read_wrapper;
    source->write = mem_write_wrapper;
    source->move = mem_move_wrapper;
    source->pos = mem_pos_wrapper;
    source->set_read_callback = mem_set_read_callback_wrapper;
    source->set_write_callback = mem_set_write_callback_wrapper;
    source->set_read_full_callback = mem_set_read_full_callback_wrapper;
    source->set_write_full_callback = mem_set_write_full_callback_wrapper;
    return source;
}

multi_data_source * create_multi_data_source(data_source **source_list, size_t count) {
    if (!source_list || count <= 0) {
        return NULL;
    }
    multi_data_source *source = malloc(sizeof(multi_data_source));

    multi_data_source_member *member = malloc(sizeof(multi_data_source_member));
    member->source_list = source_list;
    member->source_count = count;
    member->cur_pos = 0;
    member->cur_source_index = 0;
    member->total_size = 0;
    int i;
    for (i = 0; i < count; i++) {
        member->total_size += (source_list[i])->size;
    }

    source->member = member;
    source->read = multi_read_wrapper;
    source->write = multi_write_wrapper;
    source->move = multi_move_wrapper;
    source->get_source = multi_get_source_wrapper;
    source->get_source_count = multi_get_source_count_wrapper;
    source->size = multi_size_wrapper;
    source->resize = multi_resize_wrapper;
    return source;
}

void destroy_multi_data_source(multi_data_source *multi) {
    if (!multi) {
        return;
    }
    int i;
    for (i = 0; i < multi->get_source_count(multi); i++) {
        data_source *source = multi->get_source(multi, i);
        if (source) {
            free(source->data);
        }
        free(source);
    }
    free(multi->member);
    free(multi);
}

// =============== data_source相关代码[end] =============== //