//
// Created by jason on 2016/1/14.
//
/**
 * secret_struct.h
 *
 * This file defines the structure and data specification of secret message,
 * including secret content and secret meta data.
 * Also this file provide some methods to create/recycle secret structure and crc calculation.
 */

#ifndef SECRET_SECRET_H
#define SECRET_SECRET_H


#ifndef SECRET_META_LENGTH
#define SECRET_META_LENGTH 24 // meta信息长度(单位:字节)
#endif

#ifndef SECRET_CRC_LENGTH
#define SECRET_CRC_LENGTH 4 // crc校验码长度(单位:字节)
#endif

typedef struct {
    unsigned char type[2];     // 类型(第二个字节为奇数表示包含key，为偶数表示不含key)
    char *key;                  // md5编码后的密钥
    unsigned long crc;         // crc校验码(32 bits or more)
} secret_meta;

/**
 * Secret带Meta信息的数据格式(所有数据类型均以Big-Endian字节序存储)。
 * -------------------------------------------------------------------------------------------------
 * | Tag:SE(2 Byte) | size(4 Byte) | type(2 Byte) | key(16 Byte) | data(size Byte) | crc32(4 Byte) |
 * -------------------------------------------------------------------------------------------------
 * |<--------------------------- meta -------------------------->|<----- data ---->|<--- crc32 --->|
 * 注意：如果key字段都为0时，表示该字段为空，而不是表示该字段的值为0；
 */
typedef struct {
    char *file_path;        // data数据的文件路径
    unsigned char *data;   // data数据的内存指针
    size_t size;            // data数据的大小
    secret_meta *meta;      // meta信息
} secret;

/**
 * 动态创建并初始化secret结构体。
 * @param meta 是否包含meta数据，不包含为0，包含为非0。
 * @return 返回secret结构体。
 */
secret *secret_create(int meta);

/**
 * 回收secret结构体。
 * @param se secret结构体。
 */
void secret_destroy(secret *se);

/**
 * 解析数据流中的meta字节数据，并转化为secret_meta结构体(同时进行格式验证)。
 * @param data secret数据
 * @param length secret数据大小(大于等于SECRET_META_LENGTH字节才能完整解析)
 * @param result 解析的meta数据存放在secret结构体(其中的meta字段不能为空)
 * @return 如果解析成功，则返回1；
 *         如果字节不够，则返回0；
 *         如果解析失败，则返回-1。
 */
int secret_get_meta(unsigned char *data, size_t length, secret *result);

/**
 * 根据secret结构体创建24字节的meta字节数据。
 * @param se secret结构体。
 * @param result 如果secret的key为空，则结果为8字节的meta数据；
 *               如果secret的key不为空，则结果为24字节的meta数据。
 * @return 如果创建成功，返回meta数据的大小；如果创建失败，返回-1。
 */
int secret_create_meta(secret *se, unsigned char **result);


/**
 * 计算crc校验码。
 * @param crc
 * @param data
 * @param length
 * @return 返回计算的crc校验码。
 */
unsigned long secret_cal_crc2(unsigned long crc,
                              unsigned char *data,
                              size_t length);

/**
 * 计算crc校验码。
 * 注意：传入的secret结构体的se->meta->crc并不会被修改。
 * @param se
 * @return 返回计算的crc校验码。
 */
unsigned long secret_cal_crc(secret *se);

/**
 * 验证crc校验码。
 * @param crc1
 * @param crc2
 * @return 验证crc校验码一致，返回0；不一致返回非0；
 */
int secret_check_crc(unsigned long crc1, unsigned char* crc2);

#endif //SECRET_SECRET_H
