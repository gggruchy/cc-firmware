#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "widget.h"

/**
 * @brief 初始化信息，获取字体、语言、翻译数量
 * 
 * @param info 
 * @param csv 
 * @param bin 
 * @return int 
 */
int widget_init(widget_info_t *info, const char *csv, const char *bin)
{
    char line_buf[4096];
    char *line_token;
    char *line_saveptr;
    int line, unit;
    uint32_t index, weight, color;

    char *param_token;
    char *param_saveptr;
    int param_len = 0;

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

    //读取CSV填充结构体数据
    fseek(info->csv, 0, SEEK_SET);
    line = 0;

    int win = 0;
    int current_win_index = 0;
    int current_widget_index = 0;
    //初始化窗口列表
    int window_list[256];
    int window_numbers = 0;
    int total_widget_numbers = 0;
    for (int i = 0; i < 256; i++)
        window_list[i] = -1;
    int max_widget_numbers = -1;
    while (fgets(line_buf, sizeof(line_buf), info->csv) != NULL)
    {
        if (line > 0)
        {
            unit = 0;
            line_token = strtok_r(line_buf, "\t", &line_saveptr);
            int prop_len = 0;
            while (line_token != NULL)
            {
                switch (unit)
                {
                case 0: //注释
                    break;
                case 1: //窗口序号
                    sscanf(line_token, "%d", &win);
                    //记录到窗口列表中用于统计窗口数量
                    int i;
                    for (i = 0; i < 256; i++)
                    {
                        if (window_list[i] == -1 || window_list[i] == win)
                            break;
                    }
                    //数量超出256
                    if (i >= 256)
                    {
                        printf("window numbers >= 256\n");
                        return -1;
                    }
                    if (window_list[i] == -1) //新窗口
                    {
                        window_list[i] = win;
                        // printf("new window = %d\n", win);
                        info->window_header[window_numbers].window_index = win;
                        window_numbers++;
                    }
                    for (i = 0; i < window_numbers; i++)
                    {
                        if (info->window_header[i].window_index == win)
                        {
                            current_win_index = i;
                            break;
                        }
                    }
                    current_widget_index = info->window_header[current_win_index].widget_numbers;
                    info->window_header[current_win_index].widget_numbers++;
                    if (max_widget_numbers < info->window_header[current_win_index].widget_numbers)
                        max_widget_numbers = info->window_header[current_win_index].widget_numbers;
                    total_widget_numbers++;
                    break;
                case 2:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].index);
                    break;
                case 3:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].type);
                    break;
                case 4:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].x);
                    break;
                case 5:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].y);
                    break;
                case 6:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].w);
                    break;
                case 7:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].h);
                    break;
                case 8:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].callback_index);
                    break;
                case 9:
                    sscanf(line_token, "%hu", &info->widget_header[current_win_index][current_widget_index].event_index);
                    break;
                case 10:
                    param_token = strtok_r(line_token, ":", &param_saveptr);
                    param_len = 0;
                    while (param_token != NULL)
                    {
                        if (sscanf(param_token, "%x|%x",
                                   &info->widget_flag_header[current_win_index][current_widget_index][param_len].flag,
                                   &info->widget_flag_header[current_win_index][current_widget_index][param_len].mask))
                            param_len++;
                        param_token = strtok_r(NULL, ":", &param_saveptr);
                    }
                    info->widget_header[current_win_index][current_widget_index].widget_flag_numbers = param_len;
                    break;
                default:
                    if (line_token != NULL)
                    {
                        if (sscanf(line_token, "%d|#%x|%x",
                                   &info->widget_style_header[current_win_index][current_widget_index][prop_len].prop,
                                   &info->widget_style_header[current_win_index][current_widget_index][prop_len].value,
                                   &info->widget_style_header[current_win_index][current_widget_index][prop_len].selector) == 3)
                            prop_len++;
                        else if (sscanf(line_token, "%d|%d|%x",
                                        &info->widget_style_header[current_win_index][current_widget_index][prop_len].prop,
                                        &info->widget_style_header[current_win_index][current_widget_index][prop_len].value,
                                        &info->widget_style_header[current_win_index][current_widget_index][prop_len].selector) == 3)
                            prop_len++;
                    }
                    break;
                }
                line_token = strtok_r(NULL, "\t", &line_saveptr);
                unit++;
            }
            info->widget_header[current_win_index][current_widget_index].widget_style_numbers = prop_len;
        }
        line++;
    }

    info->bin_header.magic = WIDGET_MAGIC;
    info->bin_header.window_numbers = window_numbers;
    info->bin_header.total_widget_numbers = total_widget_numbers;
    info->bin_header.max_widget_numbers = max_widget_numbers;
    printf("total window numbers: %d\n", window_numbers);
    printf("max widget numbers: %d\n", max_widget_numbers);
    //偏移到控件起始位置
    fseek(info->bin, sizeof(widget_bin_header_t) + info->bin_header.window_numbers * sizeof(window_header_t), SEEK_SET);

    for (int i = 0; i < info->bin_header.window_numbers; i++)
    {
        printf("window %d widget numbers %d\n", info->window_header[i].window_index, info->window_header[i].widget_numbers);
        //记录offset
        info->window_header[i].widget_offset = ftell(info->bin);
        for (int j = 0; j < info->window_header[i].widget_numbers; j++)
        {
            // printf("index %d type %d x %d y %d w %d h %d callback %d event %d flag_numbers %d style_numbers %d\n",
            //        info->widget_header[i][j].index, info->widget_header[i][j].type,
            //        info->widget_header[i][j].x, info->widget_header[i][j].y, info->widget_header[i][j].w, info->widget_header[i][j].h,
            //        info->widget_header[i][j].callback_index, info->widget_header[i][j].event_index,
            //        info->widget_header[i][j].widget_flag_numbers, info->widget_header[i][j].widget_style_numbers);
            //写入控件头部
            fwrite(&info->widget_header[i][j], 1, sizeof(widget_header_t), info->bin);
            //写入FLAG数据
            for (int k = 0; k < info->widget_header[i][j].widget_flag_numbers; k++)
                fwrite(&info->widget_flag_header[i][j][k], 1, sizeof(widget_flag_header_t), info->bin);
            //写入STYLE数据
            for (int k = 0; k < info->widget_header[i][j].widget_style_numbers; k++)
                fwrite(&info->widget_style_header[i][j][k], 1, sizeof(widget_style_header_t), info->bin);
        }
    }

    //写WINDOW HEADER数据
    fseek(info->bin, sizeof(widget_bin_header_t), SEEK_SET);
    fwrite(info->window_header, info->bin_header.window_numbers, sizeof(window_header_t), info->bin);

    //写BIN HEADER数据
    fseek(info->bin, 0, SEEK_SET);
    fwrite(&info->bin_header, 1, sizeof(widget_bin_header_t), info->bin);
    fclose(info->csv);
    fclose(info->bin);
    return 0;
}

int widget_parse(const char *path)
{
    FILE *fp = fopen(path, "rb");
    widget_bin_header_t widget_bin_header;
    window_header_t window_header;
    widget_header_t widget_header;
    widget_flag_header_t widget_flag_header;
    widget_style_header_t widget_style_header;

    fseek(fp, 0, SEEK_SET);
    fread(&widget_bin_header, 1, sizeof(widget_bin_header_t), fp);
    printf("bin header: magic %x size %d crc %d window_numbers %d\n", widget_bin_header.magic, widget_bin_header.size, widget_bin_header.crc, widget_bin_header.window_numbers);

    for (int i = 0; i < widget_bin_header.window_numbers; i++)
    {
        fseek(fp, i * sizeof(window_header_t) + sizeof(widget_bin_header_t), SEEK_SET);
        fread(&window_header, 1, sizeof(window_header_t), fp);
        printf("window_header(%d):\n", i);
        printf("widget_offset %d window_attr %d widget_numbers %d window_index %d \n",
               window_header.widget_offset, window_header.window_attr, window_header.widget_numbers, window_header.window_index);

        fseek(fp, window_header.widget_offset, SEEK_SET);
        for (int j = 0; j < window_header.widget_numbers; j++)
        {
            fread(&widget_header, 1, sizeof(widget_header_t), fp);
            printf("widget_header(%d)(%d):\n", i, j);
            printf("index %d type %d x %d y %d w %d h %d callback %d event %d flag_numbers %d style_numbers %d\n",
                   widget_header.index, widget_header.type, widget_header.x, widget_header.y, widget_header.w, widget_header.h,
                   widget_header.callback_index, widget_header.event_index, widget_header.widget_flag_numbers, widget_header.widget_style_numbers);
            printf("\n");

            for (int k = 0; k < widget_header.widget_flag_numbers; k++)
            {
                fread(&widget_flag_header, 1, sizeof(widget_flag_header_t), fp);
                printf("flag %x mask %x\n", widget_flag_header.flag, widget_flag_header.mask);
            }

            for (int k = 0; k < widget_header.widget_style_numbers; k++)
            {
                fread(&widget_style_header, 1, sizeof(widget_style_header_t), fp);
                printf("prop %d value %x value %d selector %x\n",
                       widget_style_header.prop, widget_style_header.value, widget_style_header.value, widget_style_header.selector);
            }
        }
        printf("\n");
    }
    fclose(fp);
}
