//
// Created by jason on 2016/1/7.
//

#ifndef SECRET_SECRET_UTIL_H
#define SECRET_SECRET_UTIL_H

/**
 * 检验指定文件是否png图片。
 * @param file_path 文件路径
 * @return 如果是png图片，返回非0；如果不是png，则返回0。
 */
int check_png(char *file_path);

/**
 * 检验指定文件是否png图片。
 * 注意：该方法调用完毕，不会关闭file，并且文件指针会指回文件开头。
 * @param file_path 文件路径
 * @return 如果是png图片，返回非0；如果不是png，则返回0。
 */
int check_png2(FILE *file);

/**
 * 获取指定颜色类型下，一个颜色像素所占用的字节数。
 * @param color_type 颜色类型(目前只支持0,2,4,6)
 * @return 返回颜色像素所占用的字节数；如果颜色类型不支持或不识别，则返回0。
 */
size_t get_color_bytes(unsigned char color_type);

/**
 * 在当前扫描层数，判断一行内的字节索引位置，是否是ada7采样的有效位(默认已经是在有效行内)。
 * @param index 行内的字节索位置
 * @param pass 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @param color_type png图片的颜色类型(0,2,4,6)。
 * @return 如果是有效位，返回1；不是则返回0。
 */
int col_is_adam7(int index, int pass, unsigned char color_type);

/**
 * 在当前扫描层数，判断某一行是否是ada7采样的有效行。
 * @param index 行索引
 * @param pass 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @param color_type png图片的颜色类型(0,2,4,6)。
 * @return 如果是有效行，返回1；不是则返回0。
 */
int row_is_adam7(int index, int pass);

/**
 * 在当前扫描层数，获取该行中ada7采样的有效位字节数(默认已经是在有效行内)。
 * @param row_byte 当前该行的总字节数
 * @param pass 当前间隔扫描的层数(Adam7算法)。-1表示该png没有启用间隔扫描。
 * @param color_type png图片的颜色类型(0,2,4,6)。
 * @return 如果是有效行，返回1；不是则返回0。
 */
size_t get_adam7_byte_size(size_t row_byte, int pass, unsigned char color_type);

#endif //SECRET_SECRET_UTIL_H
