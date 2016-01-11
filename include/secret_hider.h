//
// Created by jason on 2016/1/6.
//

#ifndef SECRET_HIDER_H
#define SECRET_HIDER_H

#include <stddef.h>

/**
 * 隐藏信息进宿主数据过程中，宿主数据的不满8的倍数的余数
 */
typedef struct _remainder {
    unsigned char data[7];
    size_t size;
} remainder;

typedef struct _filter {
    void *param;// 用于过滤数据的相关参数
    int (*is_effective)(int, void *);// 判断当前索引位置的数据是否有效
    size_t (*get_effective_size)(size_t, void *);// 获取给定数据内有效数据的总数
} filter;

/**
 * 获取指定大小的data中包含的secret字节数。
 * @data_size data的字节大小
 * @return 返回data中包含的secret字节数
 */
size_t contains_secret_bytes(size_t data_size);

/**
 * 往data中写入secret信息(1byte的data数据包含1bit的secret信息，写入过程要算上remain中包含了不足1字节的secret信息）
 * ，因此data的长度应该是8的倍数；
 * 如果长度不是8的倍数，则把最后不足8的data数据存入remain中(如果secret已全部写入，remain会清0)，以方便下次调用该方法。
 * @return：如果data为空，则返回-1；其他写入成功的情况的下，返回此次写入的secret字节数。
 */
int write_secret_to_data(unsigned char *data_p, int data_offset, size_t data_size,
                         char *secret_p, int secret_offset, size_t secret_size,
                         remainder *remain, filter* f);

/**
 * 从remain+data中提取secret信息(1byte的data数据包含1bit的secret信息)，因此remain+data的长度应该是8的倍数；
 * 如果长度不是8的倍数，则把最后不足8的data数据存入remain中(如果secret已全部读取，remain会清0)，以方便下次调用该方法。
 * @pass: 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @return：如果data为空，则返回-1；其他提取成功的情况的下，返回此次提取的secret字节数。
 */
int read_secret_from_data(unsigned char *data_p, int data_offset, size_t data_size,
                          char *secret_p, int secret_offset, size_t secret_size,
                          remainder *remain, filter* f);

#endif //SECRET_HIDER_H
