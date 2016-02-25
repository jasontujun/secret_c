//
// Created by jason on 2016/1/7.
//

#ifndef SECRET_SECRET_UTIL_H
#define SECRET_SECRET_UTIL_H

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif


/**
 * 将size_t转换为字节数组(采取Big-Endian字节序)。
 * @param integer size_t的值
 * @param bytes 字节数组，转换的结果
 * @param length 字节个数(大于4，等同于4)
 * @return 转换失败返回0；转换成功返回非0。
 */
int sizet_to_byte(size_t integer, unsigned char* bytes, size_t length);

/**
 * 将字节数组转换为size_t(采取Big-Endian字节序)。
 * @param bytes 字节数组
 * @param length 字节个数(大于4，等同于4)
 * @return 返回转换后的size_t。
 */
size_t byte_to_sizet(unsigned char* bytes, size_t length);

/**
 * 将unsigned long转为字节数组。
 * @param integer unsigned long的值
 * @param bytes 字节数组，转换的结果
 * @param length 字节个数(大于4，等同于4)
 * @return 转换失败返回0；转换成功返回非0。
 */
int ulong_to_byte(unsigned long integer, unsigned char* bytes, size_t length);

/**
 * 根据给定的文件头标识，判断文件是否符合指定的文件格式。
 * @param file
 * @param signature
 * @param bytes_to_check
 * @return 符合返回0；不符合返回非0。
 */
int check_file_format(FILE* file, void *signature, size_t bytes_to_check);


/**
 * 该结构体定义了数据源的接口。
 * 数据源支持随机读取和写入操作。
 */
typedef struct _data_source {
    /**
     * 实际数据源结构的指针。可能是内存数据，也可能是文件描述符，甚至是个流。
     */
    void *data;
    /**
     * 数据的大小(无论读取还是写入，都不会超过这个大小)。
     */
    long size;
    /**
     * 从数据源中读取指定大小的数据到buffer缓冲区。
     */
    int (*read)(struct _data_source *source, size_t size, void *buffer);
    /**
     * 从buffer缓冲区写入指定大小的数据到multi_data_source中。
     */
    int (*write)(struct _data_source *source, size_t size, void *buffer);
    /**
     * 移动当前数据指针的位置，用于随机读取或写入数据。
     */
    int (*move)(struct _data_source *source, long offset);
    /**
     * 获取当前数据指针的位置。
     */
    long (*pos)(struct _data_source *source);
    /**
     * 设置回调此方法(每次读操作之后都会触发)。
     * @param source 数据源本身
     * @param param 参数(触发时会原封不动的将此值传入回调函数)
     * @param callback 回调函数(此次读入数据的指针，此次读入数据的大小，参数)
     */
    void (*set_read_callback)(struct _data_source *source, void *param,
                                   void (*callback)(void *, size_t, void *));
    /**
     * 设置回调此方法(每次写操作之后都会触发)。
     * @param source 数据源本身
     * @param param 参数(触发时会原封不动的将此值传入回调函数)
     * @param callback 回调函数(此次写入数据的指针，此次写入数据的大小，参数)
     */
    void (*set_write_callback)(struct _data_source *source, void *param,
                                    void (*callback)(void *, size_t, void *));
    /**
     * 设置回调此方法(当读操作满时会触发)。
     * @param source 数据源本身
     * @param param 参数(触发时会原封不动的将此值传入回调函数)
     * @param callback 回调函数(数据指针(可能是文件指针，也可能是内存指针等)，数据总大小，参数)
     */
    void (*set_read_full_callback)(struct _data_source *source, void *param,
                                void (*callback)(void *, long, void *));
    /**
     * 设置回调此方法(当写操作满时会触发)。
     * @param source 数据源本身
     * @param param 参数(触发时会原封不动的将此值传入回调函数)
     * @param callback 回调函数(数据指针(可能是文件指针，也可能是内存指针等)，数据总大小，参数)
     */
    void (*set_write_full_callback)(struct _data_source *source, void *param,
                                void (*callback)(void *, long, void *));
} data_source;

/**
 * 多重数据源。
 * 实际上就是将多个不同的数据源从前到后串联起来进行读取和写入。
 */
typedef struct _multi_data_source {
    void *member;
    /**
     * 获取指定索引的数据源。
     * @param _multi_data_source 数据源
     * @param int index 索引位置
     * @return 获取成功返回指定数据源；获取失败返回NULL。
     */
    data_source * (*get_source)(struct _multi_data_source *, int);
    /**
     * 获取数据源个数。
     * @param _multi_data_source 数据源
     * @return 返回数据源个数。
     */
    size_t (*get_source_count)(struct _multi_data_source *);
    /**
     * 从multi_data_source中读取指定大小的数据到buffer缓冲区。
     * @param _multi_data_source 数据源
     * @param size_t 数据大小(实际读取的数据可能小于此值)
     * @param void * buffer缓冲区
     * @return 如果读取成功，返回实际读取的数据大小；读取失败返回-1。
     */
    int (*read)(struct _multi_data_source *, size_t, void *);
    /**
     * 从buffer缓冲区写入指定大小的数据到multi_data_source中。
     * @param _multi_data_source 数据源
     * @param size_t 数据大小(实际写入的数据可能小于此值)
     * @param void * buffer缓冲区
     * @return 如果读取成功，返回实际读取的数据大小；读取失败返回-1。
     */
    int (*write)(struct _multi_data_source *, size_t, void *);
    /**
     * 将读写指针移动到指定的位置(相对于数据源的起始位置)。
     * @param _multi_data_source 数据源
     * @param int new_pos 新的位置
     * @return 成功返回0，失败返回非0值。
     */
    int (*move)(struct _multi_data_source *, long);
    /**
     * 获取数据源总大小。
     * @param _multi_data_source 数据源
     * @return 返回数据源个数。
     */
    long (*size)(struct _multi_data_source *);
    /**
     * 修改指定索引的数据源的大小。
     * 注意：只是修改对应数据源的size字段，即大小范围。
     * 并没有申请新的内存空间或磁盘空间，需要数据源原本就能满足新的大小声明。
     * @param _multi_data_source 数据源
     * @param int index 索引位置
     * @param long new_size
     * @return 更新成功返回0，失败返回非0值。
     */
    int (*resize)(struct _multi_data_source *, int, long);
} multi_data_source;

data_source * create_file_data_source(FILE *file, long size);

data_source * create_memory_data_source(unsigned char *mem, long size);

multi_data_source * create_multi_data_source(data_source **source_list, size_t count);

void destroy_multi_data_source(multi_data_source *multi);

#endif //SECRET_SECRET_UTIL_H
