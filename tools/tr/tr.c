#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tr.h"

static int comp(const void *a, const void *b);

static int weight_get_index(uint8_t *weight_list, uint8_t weight_list_len, uint8_t weight)
{
    for (int i = 0; i < weight_list_len; i++)
    {
        if (weight_list[i] == weight)
            return i;
    }
    return -1;
}

/**
 * @brief 初始化信息，获取字体、语言、翻译数量
 * 
 * @param info 
 * @param csv 
 * @param bin 
 * @return int 
 */
int tr_init(tr_info_t *info, const char *csv, const char *bin)
{
    char line_buf[4096];
    char *line_token;
    char *line_saveptr;
    int line, unit;
    uint32_t index, weight, color;
    memset(info, 0, sizeof(*info));
    info->csv = fopen(csv, "rb");
    info->bin = fopen(bin, "wb");
    if (info->csv == NULL)
    {
        printf("can't read %s\n", csv);
        return -1;
    }
    if (info->bin == NULL)
    {
        printf("can't write %s\n", bin);
        return -1;
    }

    fseek(info->csv, 0, SEEK_SET);

    line = 0;
    while (fgets(line_buf, sizeof(line_buf), info->csv) != NULL)
    {
        unit = 0;
        line_token = strtok_r(line_buf, "\t", &line_saveptr);
        while (line_token != NULL)
        {
            if (line == 0 && unit > 1)
            {
                info->lang_len++;
            }
            else if (line > 0 && unit == 1)
            {
                char *weight_token;
                char *weight_saveptr;
                weight_token = strtok_r(line_token, "-", &weight_saveptr);
                while (weight_token != NULL)
                {
                    sscanf(weight_token, "%u", &weight);
                    //记录字号大小
                    if (info->font_len == 0)
                    {
                        info->font_data[info->font_len] = weight;
                        info->font_len++;
                    }
                    else
                    {
                        int i;
                        for (i = 0; i < info->font_len; i++)
                        {
                            if (weight == info->font_data[i])
                                break;
                        }
                        if (i == info->font_len)
                        {
                            info->font_data[info->font_len] = weight;
                            info->font_len++;
                        }
                    }
                    weight_token = strtok_r(NULL, "-", &weight_saveptr);
                }
            }
            line_token = strtok_r(NULL, "\t", &line_saveptr);
            unit++;
        }
        line++;
    }
    info->tr_len = line - 1;

    //对字号大小进行排序
    qsort(info->font_data, info->font_len, sizeof(uint8_t), comp);
    printf("weight list :");
    for (int i = 0; i < info->font_len; i++)
    {
        printf("%d ", info->font_data[i]);
    }
    printf("\n");
    return 0;
}

void tr_deinit(tr_info_t *info)
{
    fclose(info->csv);
    fclose(info->bin);
}

static void replace_line(char *str)
{
    int len = strlen(str);
    for (int i = 0; i < len - 1; i++)
    {
        if (str[i] == '\\' && str[i + 1] == 'n')
        {
            str[i] = '\n';

            for (int j = i + 1; j <= len; j++)
                str[j] = str[j + 1];
        }
    }
}

/**
 * @brief 处理信息的写入
 * 
 * @param info 
 * @return int 
 */
int tr_handle(tr_info_t *info)
{
    printf("tr_handle\n");
    char line_buf[4096];
    char *line_token;
    char *line_saveptr;

    int line, unit;
    uint32_t index;
    uint32_t color;
    uint32_t weight_list[255];
    char **str = malloc(info->lang_len * info->tr_len * sizeof(char *));
    if (str == NULL)
        return -1;
    tr_header_t *header = malloc(info->lang_len * info->tr_len * sizeof(tr_header_t));
    if (header == NULL)
        return -1;
    fseek(info->csv, 0, SEEK_SET);
    line = 0;
    int pos;
    //拷贝数据到内存中
    while (fgets(line_buf, sizeof(line_buf), info->csv) != NULL)
    {
        if (line > 0)
        {
            unit = 0;
            line_token = strtok_r(line_buf, "\t", &line_saveptr);

            while (line_token != NULL)
            {
                if (unit == 0) //字符串序号
                {
                    sscanf(line_token, "%u", &index);
                    printf("line_token = %s line_token_len = %d index %d\n", line_token, strlen(line_token), index);
                    index -= 1;
                }
                else if (unit == 1) //字号大小，不同语言使用不同的字号。使用‘-’分割
                {
                    char *weight_token;
                    char *weight_saveptr;
                    int lang_index = 0;
                    // printf("line_token = %s\n", line_token);
                    weight_token = strtok_r(line_token, "-", &weight_saveptr);
                    while (weight_token != NULL)
                    {
                        sscanf(weight_token, "%u", &weight_list[lang_index]);
                        lang_index++;
                        weight_token = strtok_r(NULL, "-", &weight_saveptr);
                    }
                    if (lang_index == 0)
                    {
                        printf("can't find lang weight\n");
                        return -1;
                    }
                    while (lang_index < info->lang_len)
                    {
                        weight_list[lang_index] = weight_list[lang_index - 1];
                        lang_index++;
                    }
                }
                else //翻译数据
                {
                    pos = (unit - 2) * info->tr_len + index;
                    // printf("pos %d unit %d tr_len %d index %d\n", pos, unit, info->tr_len, index);
                    //处理文本中包括换行
                    replace_line(line_token);
                    header[pos].len = strlen(line_token) + 1;
                    header[pos].weight = weight_list[unit - 2];
                    header[pos].weight_index = weight_get_index(info->font_data, info->font_len, header[pos].weight);
                    str[pos] = malloc(header[pos].len);
                    strncpy(str[pos], line_token, header[pos].len);
                    if (pos == 0)
                        printf("pos unit %d tr_len %d index %d\n", unit, info->tr_len, index);
                }
                line_token = strtok_r(NULL, "\t", &line_saveptr);
                unit++;
            }
        }
        line++;
    }

    //|header|font_weight_list|lang_offset_list|header_list|data|

    //偏移到数据区开始写入数据并记录文件偏移
    fseek(info->bin, sizeof(tr_bin_header_t) + info->font_len * sizeof(uint8_t) + info->lang_len * sizeof(uint32_t) + info->lang_len * info->tr_len * sizeof(tr_header_t), SEEK_SET);
    for (int i = 0; i < info->lang_len; i++)
    {
        for (int j = 0; j < info->tr_len; j++)
        {
            pos = i * info->tr_len + j;
            header[pos].offset = ftell(info->bin);
            fwrite(str[pos], 1, header[pos].len, info->bin);
            // printf("offset %x lang %d tr %d len %d str %s \n", header[pos].offset, i, j, header[pos].len, str[pos]);
        }
    }

    //开始写入翻译头部并记录文件偏移
    fseek(info->bin, sizeof(tr_bin_header_t) + info->font_len * sizeof(uint8_t) + info->lang_len * sizeof(uint32_t), SEEK_SET);
    for (int i = 0; i < info->lang_len; i++)
    {
        info->lang_data[i] = ftell(info->bin);
        // printf("lang = %d offset %x\n", i, info->lang_data[i]);
        for (int j = 0; j < info->tr_len; j++)
        {
            pos = i * info->tr_len + j;
            fwrite(&header[pos], 1, sizeof(tr_header_t), info->bin);
        }
    }

    //开始写入翻译偏移数据
    fseek(info->bin, sizeof(tr_bin_header_t) + info->font_len * sizeof(uint8_t), SEEK_SET);
    fwrite(info->lang_data, 1, info->lang_len * sizeof(uint32_t), info->bin);

    //开始写入字体大小列表
    fseek(info->bin, sizeof(tr_bin_header_t), SEEK_SET);
    // printf("font list: ");
    // for (int i = 0; i < info->font_len; i++)
    //     printf("%d ", info->font_data[i]);
    // printf("\n");
    fwrite(info->font_data, 1, info->font_len * sizeof(uint8_t), info->bin);

    tr_bin_header_t bin_header;

    //写入头部
    bin_header.magic = TR_MAGIC;
    fseek(info->bin, 0, SEEK_END);
    bin_header.size = ftell(info->bin) - sizeof(tr_bin_header_t);
    bin_header.crc = 0;
    bin_header.font_len = info->font_len;
    bin_header.lang_len = info->lang_len;
    bin_header.tr_len = info->tr_len;

    fseek(info->bin, 0, SEEK_SET);
    fwrite(&bin_header, 1, sizeof(tr_bin_header_t), info->bin);

    // //释放内存
    for (int i = 0; i < info->lang_len * info->tr_len; i++)
        free(str[i]);
    free(header);
    free(str);
    return 0;
}

int tr_parse(const char *path)
{
    FILE *fp = fopen(path, "rb");
    tr_bin_header_t bin_header;
    tr_header_t tr_header;
    uint8_t font_data[256];
    uint32_t lang_data[256];
    char str[1024];
    fread(&bin_header, 1, sizeof(tr_bin_header_t), fp);
    printf("magic %x size %d crc %d font len %d lang len %d tr len %d\n", bin_header.magic, bin_header.size, bin_header.crc, bin_header.font_len, bin_header.lang_len, bin_header.tr_len);

    fread(font_data, 1, bin_header.font_len * sizeof(uint8_t), fp);
    printf("font list:");
    for (int i = 0; i < bin_header.font_len; i++)
    {
        printf("%d ", font_data[i]);
    }
    printf("\n");

    fread(lang_data, 1, bin_header.lang_len * sizeof(uint32_t), fp);
    printf("lang offset list:");
    for (int i = 0; i < bin_header.lang_len; i++)
    {
        printf("%.8x ", lang_data[i]);
    }
    printf("\n");

    for (int i = 0; i < bin_header.lang_len; i++)
    {
        printf("current lang = %d\n", i);

        for (int j = 0; j < bin_header.tr_len; j++)
        {
            fseek(fp, lang_data[i] + j * sizeof(tr_header_t), SEEK_SET);
            fread(&tr_header, 1, sizeof(tr_header_t), fp);
            fseek(fp, tr_header.offset, SEEK_SET);
            fread(str, 1, tr_header.len, fp);
            printf("str = %s offset = %x weight = %d weight_index = %d\n", str, tr_header.offset, tr_header.weight, tr_header.weight_index);
        }
    }
}

static int comp(const void *a, const void *b)
{
    return *(uint8_t *)a - *(uint8_t *)b;
}