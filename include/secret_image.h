//
// Created by jason on 2016/1/6.
//

#ifndef SECRET_IMAGE_H
#define SECRET_IMAGE_H

#include <stddef.h>

/**
 * Secret带Meta数据的格式。
 * -----------------------------------------------------------------
 * | SECRET(6 Byte) | size(8 Byte) | data(size Byte) | crc(2 Byte) |
 * -----------------------------------------------------------------
 */
typedef struct _secret {
    char *file_path;
    char *data;
    size_t size;
} secret;

/**
 * 动态创建secret结构体对象。
 */
secret * create_secret();

/**
 * 回收secret结构体对象。
 */
void free_secret(secret *se);

/**
 * 从给定的图片文件中直接提取secret信息[不验证Secret的Meta信息]。
 * 如果secret结构体指定了file_path的值，则会将secret数据保存为文件，
 * 否则会将secret数据保存到secret.data指向的内存中。
 * 如果secret结构体指定了size的值(大于0)，则表示期望提取的secret字节数，
 * 该方法只会提取期望大小的secret信息；
 * 如果没有指定secret结构体的size的值(默认为0)，secret信息会全部提取，
 * 并将实际提取的secret字节数赋值到size变量。
 * @param image_file 图片文件的绝对路径(非空)。
 * @param se secret结构体，表示secret信息(非空)。
 * @return 如果提取成功，返回提取secret信息的字节数(大于等于0)；
 * 如果提取失败，返回对应的错误码(小于0)：
 *      -1：传入参数为空或非法；
 *      -2：secret文件写方式打开失败；
 *      -3：图片文件读方式打开失败；
 *      -4：图片文件格式错误，不是PNG；
 *      -5：图片文件读操作出错；
 *      -6：图片interlace属性不合法，无法解析；
 *      -7：图片color_type属性不合法，无法解析；
 *      -8：图片能包含的最大secret字节数小于要求的期望字节数；
 *      -9：secret存储为文件时出错；
 */
int read_image_directly(const char *image_file, secret *se);

/**
 * 直接将secret信息写入指定的图片文件中[不添加Secret的Meta信息]。
 * 如果secret结构体指定了file_path的值，表示secret信息存储在文件中，
 * 该方法会从文件中加载secret信息再写入图片中；
 * 否则会认为secret信息存储在data指向的内存中，直接将内存的secret信息写入图片中。
 * @param image_input_file 输入图片文件的绝对路径(非空)。
 * @param image_output_file 输出图片文件的绝对路径(非空)。
 * @param se secret结构体，表示secret信息(非空)。
 * @param min_size 最小写入字节数。如果该值小于等于0，表示没有最小写入字节数限制，
 * 能写入多少secret信息就写入多少，否则表示至少写入指定字节数的secret信息。
 * @return 如果写入成功，返回写入secret信息的字节数(大于等于0)；
 * 如果写入失败，返回对应的错误码(小于0)：
 *      -1：传入参数为空或非法；
 *      -2：secret文件读方式打开失败；
 *      -3：输入图片文件读方式打开失败；
 *      -4：输入图片文件格式错误，不是PNG；
 *      -5：输出图片文件写方式打开失败；
 *      -6：输入图片文件读操作出错；
 *      -7：输出图片文件写操作出错；
 *      -8：输入图片interlace属性不合法，无法解析；
 *      -9：输入图片color_type属性不合法，无法解析；
 *      -10：输入图片能包含的最大secret字节数小于指定的最小写入字节数；
 *      -11：secret文件大小为0；
 *      -12：secret文件读操作出错；
 */
int write_image_directly(const char *image_input_file,
                         const char *image_output_file,
                         secret *se, size_t min_size);

/**
 * 从给定的图片文件中提取secret信息[验证Secret的Meta信息]。
 * 如果secret结构体指定了file_path的值，则会将secret数据保存为文件，
 * 否则会将secret数据保存到secret.data指向的内存中。
 * 如果secret结构体指定了size的值(大于0)，表示期望提取的secret字节数，
 * 该方法会忽略指定的size值，根据Meta信息里的大小来提取；
 * @param image_file 图片文件的绝对路径(非空)。
 * @param se secret结构体，表示secret信息(非空)。
 * @return 如果提取成功，返回提取secret信息的字节数(大于等于0)；
 * 如果提取失败，返回对应的错误码(小于0)：
 *      -1：传入参数为空或非法；
 *      -2：secret文件写方式打开失败；
 *      -3：图片文件读方式打开失败；
 *      -4：图片文件格式错误，不是PNG；
 *      -5：图片文件读操作出错；
 *      -6：图片interlace属性不合法，无法解析；
 *      -7：图片color_type属性不合法，无法解析；
 *      -8：图片能包含的最大secret字节数小于要求的期望字节数；
 *      -9：secret存储为文件时出错；
 */
int read_image_secretly(const char *image_file, secret *se);

/**
 * 将secret信息写入指定的图片文件中[添加Secret的Meta信息]。
 * 如果secret结构体指定了file_path的值，表示secret信息存储在文件中，
 * 该方法会从文件中加载secret信息再写入图片中；
 * 否则会认为secret信息存储在data指向的内存中，直接将内存的secret信息写入图片中。
 * @param image_input_file 输入图片文件的绝对路径(非空)。
 * @param image_output_file 输出图片文件的绝对路径(非空)。
 * @param se secret结构体，表示secret信息(非空)。
 * @param min_size 最小写入字节数。如果该值小于等于0，表示没有最小写入字节数限制，
 * 能写入多少secret信息就写入多少，否则表示至少写入指定字节数的secret信息。
 * @return 如果写入成功，返回写入secret信息的字节数(大于等于0)；
 * 如果写入失败，返回对应的错误码(小于0)：
 *      -1：传入参数为空或非法；
 *      -2：secret文件读方式打开失败；
 *      -3：输入图片文件读方式打开失败；
 *      -4：输入图片文件格式错误，不是PNG；
 *      -5：输出图片文件写方式打开失败；
 *      -6：输入图片文件读操作出错；
 *      -7：输出图片文件写操作出错；
 *      -8：输入图片interlace属性不合法，无法解析；
 *      -9：输入图片color_type属性不合法，无法解析；
 *      -10：输入图片能包含的最大secret字节数小于指定的最小写入字节数；
 *      -11：secret文件大小为0；
 *      -12：secret文件读操作出错；
 */
int write_image_secretly(const char *image_input_file,
                         const char *image_output_file,
                         secret *se, size_t min_size);

#endif //SECRET_IMAGE_H
