//
// Created by jason on 2016/1/6.
//

#ifndef SECRET_HIDER_H
#define SECRET_HIDER_H

#include <stddef.h>

/**
 * 隐藏信息进宿主数据过程中，宿主数据的不满8的倍数的余数
 */
typedef struct _secret_remainder {
    unsigned char data[7];
    size_t size;
} secret_remainder;

/**
 * 数据过滤器，用来判断给定一段数据中的哪些字节是有效的，可以用来隐藏信息。
 */
typedef struct _secret_filter {
    /**
     * 用于过滤数据的相关参数
     */
    void *param;
    /**
     * 判断当前索引位置的数据是否有效。
     * @param index [int]当前索引位置
     * @param param [void *]参数
     * @return 如果当前索引位置有效则返回1；不是则返回0。
     */
    int (*is_effective)(int, void *);
    /**
     * 获取一段数据的有效数据大小(以字节为单位)。
     * @param raw_size [size_t]数据的原始大小
     * @param param [void *]参数
     * @return 返回数据的实际有效大小。
     */
    size_t (*get_effective_size)(size_t, void *);
} secret_filter;

/**
 * 获取指定大小的data中包含的secret字节数。
 * 注意：如果data最后的几个字节不能隐藏1字节的secret数据，
 * 则data的最后几个字节不会计算在内。换句话说，就是向下取整的计算方式。
 * @data_size data的字节大小
 * @return 返回data中包含的secret字节数
 */
size_t secret_contains_bytes(size_t data_size);

/**
 * 往data中写入secret信息(1byte的data数据包含1bit的secret信息，写入过程要算上remain中包含了不足1字节的secret信息）
 * ，因此data的长度应该是8的倍数；
 * 如果长度不是8的倍数，则把最后不足8的data数据存入remain中(如果secret已全部写入，remain会清0)，以方便下次调用该方法。
 * @param data_p data数据指针
 * @param data_offset data数据的偏移位置
 * @param data_size data数据大小
 * @param secret_p secret数据指针
 * @param secret_offset secret数据偏移位置
 * @param secret_size secret数据大小
 * @param remain 余数
 * @param f 过滤器，判断data数据中每个字节的有效性，无效的字节会跳过
 * @return：如果data为空，则返回-1；其他写入成功的情况的下，返回此次写入的secret字节数。
 */
int secret_hide(unsigned char *data_p, int data_offset, size_t data_size,
                unsigned char *secret_p, int secret_offset, size_t secret_size,
                secret_remainder *remain, secret_filter *filter);

/**
 * 从remain+data中提取secret信息(1byte的data数据包含1bit的secret信息)，因此remain+data的长度应该是8的倍数；
 * 如果长度不是8的倍数，则把最后不足8的data数据存入remain中(如果secret已全部读取，remain会清0)，以方便下次调用该方法。
 * @param data_p data数据指针
 * @param data_offset data数据的偏移位置
 * @param data_size data数据大小
 * @param secret_p secret数据指针
 * @param secret_offset secret数据偏移位置
 * @param secret_size secret数据大小
 * @param remain 余数
 * @param f 过滤器，判断data数据中每个字节的有效性，无效的字节会跳过
 * @return：如果data为空，则返回-1；其他提取成功的情况的下，返回此次提取的secret字节数。
 */
int secret_dig(unsigned char *data_p, int data_offset, size_t data_size,
               unsigned char *secret_p, int secret_offset, size_t secret_size,
               secret_remainder *remain, secret_filter *filter);

#endif //SECRET_HIDER_H
