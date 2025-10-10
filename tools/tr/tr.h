#ifndef TR_H
#define TR_H
#include <stdint.h>
#include <stdio.h>

typedef struct __attribute__((packed)) tr_header_tag
{
    // uint32_t color;
    uint32_t offset;
    uint16_t len;
    uint8_t weight;
    uint8_t weight_index;
} tr_header_t;

typedef struct __attribute__((packed)) tr_bin_header_tag
{
#define TR_MAGIC 0x12fd1000
    uint32_t magic;
    uint32_t size;
    uint16_t crc;
    uint8_t font_len;
    uint8_t lang_len;
    uint16_t tr_len;
} tr_bin_header_t;

typedef struct tr_info_tag
{
    uint8_t font_len;
    uint8_t lang_len;
    uint16_t tr_len;
    uint8_t font_data[256];
    uint32_t lang_data[256];
    FILE *csv;
    FILE *bin;
} tr_info_t;
int tr_get_face(int idx);
int tr_init(tr_info_t *info, const char *csv, const char *bin);
int tr_handle(tr_info_t *info);
void tr_deinit(tr_info_t *info);
int tr_parse(const char *path);
#endif