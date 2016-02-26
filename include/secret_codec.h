//
// Created by jason on 2016/1/6.
//
/**
 * secret_codec.h
 *
 * This file defines the interface of message encoding/decoding regardless of details of secret carrier.
 * The secret's type is always unsigned char, while secret carrier's type is among SECRET_CARRIER_TYPE.
 * The secret_filter encapsulates how the secret carrier encode/decode data, while this file solves
 * how to hide/dig secret into every single data of secret carrier.
 */

#ifndef SECRET_CODEC_H
#define SECRET_CODEC_H

#include <stddef.h>

/**
 * 宿主数据的类型
 */
typedef enum {
    S_CHAR,
    S_U_CHAR,
    S_SHORT,
    S_U_SHORT,
    S_INTEGER,
    S_U_INTEGER
} SECRET_CARRIER_TYPE;

/**
 * 隐藏信息进宿主数据过程中，宿主数据的不满8的倍数的余数。
 */
typedef struct {
    size_t size;// 一定小于8(一个字节的长度)
    void *data;// 类型为SECRET_CARRIER_TYPE中的一种，大小为7
} secret_remainder;

/**
 * 数据过滤器，用来判断给定一段数据中的哪些字节是有效的，可以用来隐藏信息。
 */
typedef struct {
    /**
     * 用于过滤数据的相关参数
     */
    void *param;
    /**
     * 判断当前索引位置的数据是否有效。
     * @param index 当前索引位置
     * @param data_p 数据头指针
     * @param param 参数
     * @return 如果当前索引位置有效则返回1；不是则返回0。
     */
    int (*is_effective)(void *data_p, int index, void *param);
} secret_filter;

/**
 * 往data中写入secret信息(1byte的data数据包含1bit的secret信息，写入过程要算上remain中包含了不足1字节的secret信息）
 * ，因此data的长度应该是8的倍数；
 * 如果长度不是8的倍数，则把最后不足8的data数据存入remain中(如果secret已全部写入，remain会清0)，以方便下次调用该方法。
 * @param data_t data数据类型
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
int secret_hide(SECRET_CARRIER_TYPE data_t, void *data_p, int data_offset, size_t data_size,
                unsigned char *secret_p, int secret_offset, size_t secret_size,
                secret_remainder *remain, secret_filter *filter);

/**
 * 从remain+data中提取secret信息(1byte的data数据包含1bit的secret信息)，因此remain+data的长度应该是8的倍数；
 * 如果长度不是8的倍数，则把最后不足8的data数据存入remain中(如果secret已全部读取，remain会清0)，以方便下次调用该方法。
 * @param data_t data数据类型
 * @param data_p data数据指针
 * @param data_offset data数据的偏移位置
 * @param data_size data数据大小
 * @param secret_p secret数据指针
 * @param secret_offset secret数据偏移位置
 * @param secret_size secret数据大小(只能大不能不够，不然就会丢失secret数据)
 * @param remain 余数
 * @param f 过滤器，判断data数据中每个字节的有效性，无效的字节会跳过
 * @return：如果data为空，则返回-1；其他提取成功的情况的下，返回此次提取的secret字节数。
 */
int secret_dig(SECRET_CARRIER_TYPE data_t, void *data_p, int data_offset, size_t data_size,
               unsigned char *secret_p, int secret_offset, size_t secret_size,
               secret_remainder *remain, secret_filter *filter);

#endif //SECRET_CODEC_H
