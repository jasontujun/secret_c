//
// Created by jason on 2016/1/6.
//
/**
 * secret_file.h
 *
 * This file defines the interface in user-application level to handle different format file.
 * The method secret_register_handler() and secret_unregister_handler() provide the flexibility
 * of format expansion.
 */

#ifndef SECRET_IMAGE_H
#define SECRET_IMAGE_H

#include <stdio.h>
#include "secret_struct.h"

#define ERROR_COMMON_PARAM_NULL -1
#define ERROR_COMMON_FORMAT_NOT_SUPPORT -2
#define ERROR_COMMON_VOLUME_INSUFFICIENT -3
#define ERROR_COMMON_FILE_R_OPEN_FAIL -10
#define ERROR_COMMON_FILE_W_OPEN_FAIL -11
#define ERROR_COMMON_FILE_READ_FAIL -12
#define ERROR_COMMON_FILE_WRITE_FAIL -13
#define ERROR_COMMON_FILE_EMPTY -14
#define ERROR_COMMON_META_PARSE_FAIL -20
#define ERROR_COMMON_META_CREATE_FAIL -21
#define ERROR_COMMON_CRC_CHECK_FAIL -30
#define ERROR_COMMON_CRC_CAL_FAIL -31

/**
 * 定义了针对的特定文件格式的secret处理器所必须具备的函数接口。
 */
typedef struct {
    /**
     * 判断文件格式是否符合。
     * @param se_file 给定的文件
     * @return 符合返回0，不符合返回非0
     */
    int (*secret_file_format)(FILE *se_file);
    size_t (*secret_file_volume)(const char *se_file, int has_meta);
    int (*secret_file_meta)(const char *se_file, secret *result);
    int (*secret_file_dig)(const char *se_file, secret *se);
    int (*secret_file_hide)(const char *se_input_file, const char *se_output_file, secret *se);
} secret_file_handler;


/**
 * 初始化工作。
 */
void secret_init();

/**
 * 注册针对特定文件格式的secret_file_handler。
 * @param format 文件格式的字符串标识(不可和已注册的重复)。
 * @param handler 能解析此文件格式的handler。
 * @param override 如果重复，是否覆盖已注册的handler(覆盖为非0值，不覆盖为0)。
 * @return 添加成功返回非0；添加失败返回非0。
 */
int secret_register_handler(char* format, secret_file_handler* handler, int override);

/**
 * 注销针对特定文件格式的secret_file_handler。
 * @param format 文件格式的字符串标识。
 * @return 注销成功返回非0；注销失败返回非0。
 */
int secret_unregister_handler(char* format);

/**
 * 获取指定文件能包含的最大secret字节大小。
 * @param se_file 文件的绝对路径(非空)。
 * @param has_meta 是否计算meta信息大小(计算则设为1，否则设为0)
 * @return 返回指定文件能包含的最大secret字节大小；获取失败返回0。
 */
size_t secret_file_volume(const char *se_file, int has_meta);

/**
 * 获取指定文件的secret的meta信息。
 * @param se_file 文件的绝对路径(非空)。
 * @param result 获取的meta信息结果存放在secret结构体中。
 * @return 获取成功返回非0；获取失败返回0。
 */
int secret_file_meta(const char *se_file, secret *result);

/**
 * 从指定的图片文件中提取secret信息。
 * 1.如果secret结构体指定了file_path的值，则会将secret数据保存为文件，
 * 否则会将secret的内容数据保存到secret.data指向的内存中。
 * 2.如果secret结构体设置了meta字段，则表示在提取过程中验证meta格式信息，
 * secret的内容不含meta信息(忽略secret的size字段，根据meta里的size来提取内容)；
 * 如果meat字段为空，则会忽略meta信息，将所有数据都当成secret内容。
 * 3.如果secret结构体指定了size的值(大于0)，同时meta字段为空，
 * 则表示期望提取的secret内容字节数，该方法只会提取期望大小的secret信息；
 * 如果没有指定secret结构体的size的值(默认为0)，同时meta字段为空，
 * secret信息会全部提取，并将实际提取的secret字节数赋值到size变量。
 * @param image_file 图片文件的绝对路径(非空)。
 * @param se secret结构体，表示secret信息(非空)。
 * @return 如果提取成功，返回提取secret信息的字节数(包含meta信息和crc校验码,大于等于0)；
 *         如果提取失败，返回对应的错误码(小于0)：
 *          -1：传入参数为空或非法；
 *          -2：文件读方式打开失败；
 *          -3：文件格式错误，或文件格式不支持；
 *          -4：文件读操作出错；
 *          -5：图片interlace属性不合法，无法解析；
 *          -6：图片color_type属性不合法，无法解析；
 *          -7：文件能包含的最大secret字节数小于要求的期望字节数；
 *          -8：secret文件写方式打开失败；
 *          -9：secret存储为文件时出错；
 *          -10：meta信息格式出错
 *          -11：crc校验失败
 */
int secret_file_dig(const char *se_file, secret *se);

/**
 * 将secret信息写入指定的图片文件中。
 * 1.如果secret结构体指定了file_path的值，则会从文件中加载secret信息再写入图片中；
 * 否则会认为secret信息存储在data指向的内存中，直接将内存的secret信息写入图片中。
 * 2.如果secret结构体设置了meta字段，则会在写入过程中，添加secret的meta格式信息；
 * 如果meat字段为空，则直接写入secret内容信息，不会添加meta格式信息。
 * @param se_input_file 输入图片文件的绝对路径(非空)。
 * @param se_output_file 输出图片文件的绝对路径(非空)。
 * @param se secret结构体，表示secret信息(非空)。
 * @param min_size 最小写入字节数。如果该值小于等于0，表示没有最小写入字节数限制，
 * 能写入多少secret信息就写入多少，否则表示至少写入指定字节数的secret信息。
 * @return 如果写入成功，返回写入secret信息的字节数(包含meta信息和crc校验码,大于等于0)；
 *         如果写入失败，返回对应的错误码(小于0)：
 *          -1：传入参数为空或非法；
 *          -2：secret文件读方式打开失败；
 *          -3：输入图片文件读方式打开失败；
 *          -4：输入图片文件格式错误，或图片格式不支持；
 *          -5：输出图片文件写方式打开失败；
 *          -6：输入图片文件读操作出错；
 *          -7：输出图片文件写操作出错；
 *          -8：输入图片interlace属性不合法，无法解析；
 *          -9：输入图片color_type属性不合法，无法解析；
 *          -10：输入图片能包含的最大字节数无法满足secret大小；
 *          -11：secret文件大小为0；
 *          -12：secret文件读操作出错；
 *          -13：meta信息创建失败；
 *          -14：crc计算出错；
 */
int secret_file_hide(const char *se_input_file,
                     const char *se_output_file,
                     secret *se);

#endif //SECRET_IMAGE_H
