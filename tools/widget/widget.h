#ifndef WIDGET_H
#define WIDGET_H
#include <stdint.h>
#include <stdio.h>

#define MAX_WINDOW_NUMBERS 256
#define MAX_WIDGET_NUMBERS 512
#define MAX_FLAG 8
#define MAX_STYLE 128

typedef struct __attribute__((packed)) widget_header_tag
{
    uint16_t index;
    uint16_t type;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t callback_index;
    uint16_t event_index;
    uint16_t widget_flag_numbers;
    uint16_t widget_style_numbers;
} widget_header_t;

typedef struct __attribute__((packed)) widget_flag_header_tag
{
    uint32_t flag;
    uint32_t mask;
} widget_flag_header_t;

typedef struct __attribute__((packed)) widget_style_header_tag
{
    uint32_t prop;
    int32_t value;
    uint32_t selector;
} widget_style_header_t;

typedef struct __attribute__((packed)) widget_bin_header_tag
{
#define WIDGET_MAGIC 0x12fd1001
    uint32_t magic;
    uint32_t size;
    uint16_t crc;
    uint16_t window_numbers;
    uint16_t total_widget_numbers;
    uint16_t max_widget_numbers;
} widget_bin_header_t;

typedef struct __attribute__((packed)) window_header_tag
{
    uint32_t widget_offset;
    uint16_t window_attr;
    uint8_t widget_numbers;
    uint8_t window_index;
} window_header_t;

typedef struct widget_info_tag
{
    widget_bin_header_t bin_header;
    window_header_t window_header[MAX_WINDOW_NUMBERS];
    widget_header_t widget_header[MAX_WINDOW_NUMBERS][MAX_WIDGET_NUMBERS];
    widget_flag_header_t widget_flag_header[MAX_WINDOW_NUMBERS][MAX_WIDGET_NUMBERS][MAX_FLAG];
    widget_style_header_t widget_style_header[MAX_WINDOW_NUMBERS][MAX_WIDGET_NUMBERS][MAX_STYLE];

    FILE *csv;
    FILE *bin;
} widget_info_t;

int widget_init(widget_info_t *info, const char *csv, const char *bin);
int widget_parse(const char *path);
#endif