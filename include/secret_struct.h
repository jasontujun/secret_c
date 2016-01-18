//
// Created by jason on 2016/1/14.
//

#ifndef SECRET_SECRET_H
#define SECRET_SECRET_H

typedef struct _secret_meta {
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
 * 注意：如果Meta信息包含key字段，则type的第二个字节必须是奇数(即最末尾的bit为1)；
 *       如果Meta信息不包含key字段，则type的第二个字节必须是偶数即最末尾的bit为0)；
 */
typedef struct _secret {
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
 * 验证secret数据的meta格式。
 * 注意：传入的secret数据必须大于等于2字节，否则验证失败；
 * @param data secret数据(必须大于等于2字节)
 * @param length secret数据大小
 * @return 如果验证成功，则返回1；
 *         如果字节不够，则返回0；
 *         如果验证失败，则返回-1。
 */
int secret_check_meta(unsigned char *data, size_t length);

/**
 * 解析数据流中的meta字节数据，并转化为secret_meta结构体。
 * @param data secret数据(必须大于等于2字节)
 * @param length secret数据大小
 * @param result 解析的meta数据存放在secret结构体(其中的meta字段不能为空)
 * @return 如果解析成功，则返回meta信息字大小(字节)；
 *         如果字节不够，则返回0；
 *         如果解析失败，则返回-1。
 */
int secret_get_meta(unsigned char *data, size_t length, secret *result);

/**
 * 根据secret结构体创建8字节或24字节的meta字节数据。
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
 * @param se
 * @return 返回计算的crc校验码。
 */
unsigned long secret_cal_crc(secret *se);

#endif //SECRET_SECRET_H
