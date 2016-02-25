//
// Created by jason on 2016/1/6.
//

#include <stdlib.h>
#include <string.h>
#include "secret_file.h"

struct format_node {
    char* format;
    secret_file_handler *handler;
    struct format_node *next;
};

static struct format_node *handler_list = NULL;// 用链表保存不同格式的handler

#if SUPPORT_FORMAT_PNG
extern secret_file_handler png_handler;
#endif
#if SUPPORT_FORMAT_JPEG
extern secret_file_handler jpeg_handler;
#endif

void secret_init() {
#if SUPPORT_FORMAT_PNG
    secret_register_handler("png", &png_handler, 0);
#endif
#if SUPPORT_FORMAT_JPEG
    secret_register_handler("jpeg", &jpeg_handler, 0);
#endif
}

int secret_register_handler(char* format, secret_file_handler* handler, int override) {
    if (!format || !handler) {
        return 0;
    }
    struct format_node **cur_node = &handler_list;
    while (*cur_node) {
        if (stricmp((*cur_node)->format, format) == 0) {
            if (override) {
                (*cur_node)->handler = handler;
                return 1;
            } else {
                return 0;
            }
        }
        cur_node = &((*cur_node)->next);
    }
    struct format_node *new_node = malloc(sizeof(struct format_node));
    new_node->format = format;
    new_node->handler = handler;
    new_node->next = NULL;
    (*cur_node) = new_node;
    return 1;
}

int secret_unregister_handler(char* format) {
    struct format_node **cur_node = &handler_list;
    while (*cur_node) {
        if (stricmp((*cur_node)->format, format) == 0) {
            free(*cur_node);
            *cur_node = (*cur_node)->next;
            return 1;
        }
        cur_node = &((*cur_node)->next);
    }
    return 0;
}

static secret_file_handler *choose_handler(const char *file_path) {
    FILE *file = NULL;
    if ((file = fopen(file_path, "rb")) == NULL) {
        return NULL;
    }
    secret_file_handler *handler = NULL;
    struct format_node* node;
    for (node = handler_list; node; node = node->next) {
        if (!node->handler->secret_file_format(file)) {
            handler = node->handler;
            break;
        } else {
            rewind(file);
        }
    }
    fclose(file);
    return handler;
}

size_t secret_file_volume(const char *se_file, int has_meta) {
    secret_file_handler *handler = choose_handler(se_file);
    if (handler) {
        return handler->secret_file_volume(se_file, has_meta);
    } else {
        return 0;
    }
}

int secret_file_meta(const char *se_file, secret *result) {
    secret_file_handler *handler = choose_handler(se_file);
    if (handler) {
        return handler->secret_file_meta(se_file, result);
    } else {
        return ERROR_COMMON_FORMAT_NOT_SUPPORT;
    }
}

int secret_file_dig(const char *se_file, secret *se) {
    secret_file_handler *handler = choose_handler(se_file);
    if (handler) {
        return handler->secret_file_dig(se_file, se);
    } else {
        return ERROR_COMMON_FORMAT_NOT_SUPPORT;
    }
}

int secret_file_hide(const char *se_input_file,
                     const char *se_output_file,
                     secret *se) {
    secret_file_handler *handler = choose_handler(se_input_file);
    if (handler) {
        return handler->secret_file_hide(se_input_file, se_output_file, se);
    } else {
        return ERROR_COMMON_FORMAT_NOT_SUPPORT;
    }
}
