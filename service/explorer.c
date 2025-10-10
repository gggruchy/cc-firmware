#include "explorer.h"
#include <dirent.h>
#include "hl_common.h"
#include "hl_md5.h"

#define LOG_TAG "explorer"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define EXPLORER_ITEM_ARRAY_MAX_SIZE 512
#define EXPLORER_TOP_DIR "/media/"
#define MD5_DEBUG 1
#define MAX_HISTORY_SIZE 30
static char historyPath[NAME_MAX_LEN + 1] = "/board-resource/history.txt";

static int utils_explorer_path_to_name(char *name, const char *path);
static int utils_explorer_back_path(char *name, const char *path);
static int utils_explorer_splice_path(const char *base, const char *name, char *result);
static int utils_explorer_update_items(explorer_t *explorer);
static void *utils_explore_routine(void *arg);

void utils_init_history_list(history_item_t* history_explorer)
{
    FILE* fin;
    fin = fopen(historyPath, "r");
    if(!fin)
    {
        printf("Cannot open history file.\n");
        return;
    }
    char file_path[PATH_MAX_LEN + 1];
    uint8_t index = 0;
    while(fgets(file_path, sizeof(file_path), fin))
    {
        // printf("file_path:%s\n",  file_path);
        history_item_t *item = &history_explorer[index];
        // remove end-line character if present
        if(file_path[strlen(file_path) - 1] == '\n') 
            file_path[strlen(file_path) - 1] = '\0';
        
        if (sscanf(file_path, "%[^;]; %[^;]; %"PRIu64"; %d", item->path, item->date, &item->time_consumption, &item->print_state) == 4) {
            char* last_slash = strrchr(item->path, '/');
            item->name = last_slash? strdup(last_slash + 1) : NULL;
            item->is_exist = 0;
            // printf("history_explorer[%d].path: %s\n", index, history_explorer[index].path);
            // printf("history_explorer[%d].name: %s\n", index , history_explorer[index].name);
            // printf("history_explorer[%d].date: %s\n", index , history_explorer[index].date);
            // printf("history_explorer[%d].time_consumption: %"PRIu64"\n", index , history_explorer[index].time_consumption);
            // printf("history_explorer[%d].print_state: %d\n", index , history_explorer[index].print_state);
        }
        index++;
    }
    fclose(fin);
}

void utils_add_history(explorer_item_t *file_item, history_item_t* history_explorer)
{
    history_item_t item;
    bool is_full = false;
    uint8_t index;
    strncpy(item.path, file_item->path, sizeof(item.path));         // Assuming filePath is a global variable
    item.name = file_item->name;                                                          // Assuming fileName is a global variable
    strncpy(item.date, file_item->date, sizeof(item.date));         // Assuming filedate is a global variable
    item.time_consumption = file_item->time_consumption;  // Assuming time_consumption is a global variable
    item.print_state = file_item->print_state;                                   // Assuming print_state is a global variable

    item.is_exist = 1;

    for (index = 0; index < MAX_HISTORY_SIZE; index++){
        if (history_explorer[index].name == NULL){
            is_full = false;
            break;
        }
        else{
            is_full = true;
        }
    }

    if(is_full){
        // Shift all items one position to the right to make space at the beginning
        memmove(&history_explorer[1], &history_explorer[0], (MAX_HISTORY_SIZE - 1) * sizeof(history_item_t));

        // Insert new item at the beginning
        index = 0;
    }
    history_explorer[index] = item;

    FILE* fout = fopen(historyPath, "w"); // Assuming historyPath is a global variable
    if (fout)
    {
        for(uint8_t i = 0; i < MAX_HISTORY_SIZE; i++){
            if(history_explorer[i].name != NULL){
                fprintf(fout, "%s; %s; %"PRIu64"; %d\n", history_explorer[i].path, history_explorer[i].date, history_explorer[i].time_consumption, history_explorer[i].print_state);
            }
            else{
                break;
            }
        }
        // printf("history_explorer[%d].path: %s\n", index, history_explorer[index].path);
        // printf("history_explorer[%d].name: %s\n", index , history_explorer[index].name);
        // printf("history_explorer[%d].date: %s\n", index , history_explorer[index].date);
        // printf("history_explorer[%d].time_consumption: %"PRIu64"\n", index , history_explorer[index].time_consumption);
        // printf("history_explorer[%d].print_state: %d\n", index , history_explorer[index].print_state);
        fclose(fout);
    }

    system("sync");
}

int utils_explorer_init(explorer_t *explorer)
{
    if (UTILS_CHECK(explorer != NULL))
        return -1;
    memset(explorer, 0, sizeof(*explorer));
    sem_init(&explorer->operation_ctx.stop_sem, 0, 0);
    return 0;
}

int utils_explorer_deinit(explorer_t *explorer)
{
    if (UTILS_CHECK(explorer != NULL))
        return -1;
    if (explorer->item_array)
    {
        free(explorer->item_array);
        explorer->item_array = NULL;
    }
    sem_destroy(&explorer->operation_ctx.stop_sem);
    return 0;
}

int utils_explorer_opendir(explorer_t *explorer, const char *pathname)
{
    char path[PATH_MAX_LEN + 1];
    if (UTILS_CHECK(explorer != NULL))
        return -1;
    if (UTILS_CHECK(pathname != NULL))
        return -2;
    // 拼接路径
    int delta_depth = utils_explorer_splice_path(explorer->current_path, pathname, path);

    if (delta_depth < 0)
    {
        explorer->depth--;
    }
    else if (delta_depth > 0)
    {
        explorer->depth++;
    }
    strncpy(explorer->current_path, path, PATH_MAX_LEN);
    utils_explorer_update_items(explorer);
    return 0;
}

static int utils_explorer_global_search_recursive(explorer_t *explorer, const char *entry, const char *key, int index)
{
    DIR *dirp = NULL;
    struct dirent *dp = NULL;
    struct stat st;
    common_header_t header;
    bool hit;

    char path[PATH_MAX_LEN + 1];
    char upperKey[NAME_MAX_LEN + 1];
    char upperName[NAME_MAX_LEN + 1];
    bool save = 0;

    dirp = opendir(entry);
    if (UTILS_CHECK_ERRNO(dirp != NULL))
        return 0;
    bool top = strcmp(entry, EXPLORER_TOP_DIR) == 0;

    utils_string_toupper(key, upperKey, sizeof(upperKey));

    while ((dp = readdir(dirp)) != NULL)
    {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || (strcmp(dp->d_name, "System Volume Information") == 0) || (top && strcmp(dp->d_name, "emmc0") == 0))
            continue;

        strncpy(explorer->item_array[index].name, dp->d_name, NAME_MAX_LEN);
        snprintf(explorer->item_array[index].path, PATH_MAX_LEN, "%s%s", entry, dp->d_name);
        // stat(explorer->item_array[index].path, &st);
        // if (S_ISDIR(st.st_mode))
        if (dp->d_type == DT_DIR)
            explorer->item_array[index].is_dir = 1;
        else
            explorer->item_array[index].is_dir = 0;

        utils_string_toupper(dp->d_name, upperName, sizeof(upperName));
        hit = strstr(upperName, upperKey) != NULL;

        if (hit && !top)
        {
            if (explorer->item_array[index].is_dir == 0)
            {
                if (utils_get_header(&header, explorer->item_array[index].path) == 0)
                    explorer->item_array[index].magic = header.magic;
                else
                    LOG_I("utils_get_header failed path %s\n", explorer->item_array[index].path);
            }

            if (explorer->item_callback)
                save = explorer->item_callback(explorer, &explorer->item_array[index], EXPLORER_ITEM_CALLBACK_STATUS_CONTINUE);
            if (save)
            {
                index++;
            }
        }

        if (index < explorer->total_number)
        {
            // if (S_ISDIR(st.st_mode))
            if (explorer->item_array[index].is_dir)
            {
                snprintf(path, PATH_MAX_LEN, "%s%s/", entry, dp->d_name);
                index = utils_explorer_global_search_recursive(explorer, path, key, index);
            }
        }

        if (index >= explorer->total_number)
            break;
    }

    closedir(dirp);
    return index;
}

static void utils_explorer_count_recursive(explorer_t *explorer, const char *entry, const char *key)
{
    DIR *dirp = NULL;
    struct dirent *dp = NULL;
    struct stat st;
    bool hit;
    char path[PATH_MAX_LEN + 1];
    char upperKey[NAME_MAX_LEN + 1];
    char upperName[NAME_MAX_LEN + 1];
    explorer_item_t item;
    bool top = strcmp(entry, EXPLORER_TOP_DIR) == 0;

    dirp = opendir(entry);
    if (UTILS_CHECK_ERRNO(dirp != NULL))
        return;
    utils_string_toupper(key, upperKey, sizeof(upperKey));
    while ((dp = readdir(dirp)) != NULL)
    {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0 || (strcmp(dp->d_name, "System Volume Information") == 0) || (top && strcmp(dp->d_name, "emmc0") == 0))
            continue;

        snprintf(item.path, PATH_MAX_LEN, "%s%s", entry, dp->d_name);
        // stat(item.path, &st);
        utils_string_toupper(dp->d_name, upperName, sizeof(upperName));
        hit = strstr(upperName, upperKey) != NULL;

        // if (S_ISDIR(st.st_mode))

        if (dp->d_type == DT_DIR)
        {
            if (hit && !top)
            {
                explorer->dir_number++;
            }
            snprintf(path, PATH_MAX_LEN, "%s%s/", entry, dp->d_name);
            utils_explorer_count_recursive(explorer, path, key);
        }

        if (hit && !top)
            explorer->total_number++;
    }
    closedir(dirp);
}

int utils_explorer_global_search(explorer_t *explorer, const char *key)
{
    int index = 0;
    explorer->total_number = 0;
    explorer->dir_number = 0;
    if (explorer->item_array)
    {
        free(explorer->item_array);
        explorer->item_array = NULL;
    }
    // 统计数量
    utils_explorer_count_recursive(explorer, EXPLORER_TOP_DIR, key);

    // 入口数量限制
    if (explorer->dir_number > EXPLORER_ITEM_ARRAY_MAX_SIZE)
        explorer->dir_number = EXPLORER_ITEM_ARRAY_MAX_SIZE;
    if (explorer->total_number > EXPLORER_ITEM_ARRAY_MAX_SIZE)
        explorer->total_number = EXPLORER_ITEM_ARRAY_MAX_SIZE;

    if (explorer->total_number > 0)
        explorer->item_array = (explorer_item_t *)malloc(explorer->total_number * sizeof(explorer_item_t));

    if (!explorer->item_array)
    {
        explorer->dir_number = 0;
        explorer->total_number = 0;
        if (explorer->item_callback)
            explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_START);

        if (explorer->item_callback)
            explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_END);

        // 调用回调列表更新回调函数
        if (explorer->update_callback)
            explorer->update_callback(explorer);
        return -2;
    }

    if (explorer->item_callback)
        explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_START);

    index = utils_explorer_global_search_recursive(explorer, EXPLORER_TOP_DIR, key, index);

    if (explorer->item_callback)
        explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_END);

    // 调用回调列表更新回调函数
    if (explorer->update_callback)
        explorer->update_callback(explorer);
    return 0;
}

int utils_explorer_set_path(explorer_t *explorer, const char *path)
{
    if (UTILS_CHECK(explorer != NULL))
        return -1;
    if (UTILS_CHECK(path != NULL))
        return -2;
    if (UTILS_CHECK(strlen(path) > 0))
        return -3;
    strncpy(explorer->current_path, path, PATH_MAX_LEN);
    explorer->depth = 0;
    utils_explorer_opendir(explorer, "");
    return 0;
}

int utils_explorer_set_current_path(explorer_t *explorer, const char *path)
{
    if (UTILS_CHECK(explorer != NULL))
        return -1;
    if (UTILS_CHECK(path != NULL))
        return -2;
    if (UTILS_CHECK(strlen(path) > 0))
        return -3;
    if (strstr(path, EXPLORER_TOP_DIR) == path)
    {
        explorer->depth = 0;
        strncpy(explorer->current_path, path, PATH_MAX_LEN);
        const char *p = strlen(EXPLORER_TOP_DIR) + path;
        while (*p != '\0')
        {
            if (*p == '/')
                explorer->depth++;
            p++;
        }
        explorer->depth--;
    }
    else
    {
        return -4;
    }
    return 0;
}

int utils_explorer_get_item(explorer_t *explorer, int index, explorer_item_t **item)
{
    if (UTILS_CHECK(explorer != NULL))
        return -1;
    if (UTILS_CHECK(item != NULL))
        return -2;
    *item = explorer->item_array + index;
    return 0;
}

int utils_explore_operation_start(explorer_t *explorer, char *src_path, char *dst_path, uint8_t isDelete)
{
    int ret;
    if (UTILS_CHECK(explorer != NULL))
        return -1;

    if (explorer->opertaion_state != EXPLORER_OPERATION_IDLE)
        return 1;

    if (isDelete == 0)
        explorer->opertaion_state = EXPLORER_OPERATION_COPYING;
    else if (isDelete == 1)
        explorer->opertaion_state = EXPLORER_OPERATION_MOVING;
    else if (isDelete == 2)
        explorer->opertaion_state = EXPLORER_OPERATION_DELETE;

    if (explorer->opertaion_state == EXPLORER_OPERATION_COPYING || explorer->opertaion_state == EXPLORER_OPERATION_MOVING)
    {
        strncpy(explorer->operation_ctx.dst_path, dst_path, PATH_MAX_LEN);
        strncpy(explorer->operation_ctx.src_path, src_path, PATH_MAX_LEN);
    }
    else if (explorer->opertaion_state == EXPLORER_OPERATION_DELETE)
    {
        strncpy(explorer->operation_ctx.dst_path, dst_path, PATH_MAX_LEN);
    }

    if (pthread_create(&explorer->operation_ctx.thread, NULL, utils_explore_routine, explorer) < 0)
    {
        ret = -5;
        goto fail;
    }

    if (explorer->opertaion_state == EXPLORER_OPERATION_DELETE_DONE)
    {
        utils_explorer_update_items(explorer);
    }

    return 0;
fail:
    explorer->opertaion_state = EXPLORER_OPERATION_IDLE;
    return ret;
}

int utils_explore_operation_stop(explorer_t *explorer)
{
    if (explorer->opertaion_state != EXPLORER_OPERATION_COPYING && explorer->opertaion_state != EXPLORER_OPERATION_MOVING)
        return -1;
    sem_post(&explorer->operation_ctx.stop_sem);
    pthread_join(explorer->operation_ctx.thread, NULL);
    return 0;
}

static int utils_explorer_path_to_name(char *name, const char *path)
{
    if (UTILS_CHECK(path != NULL))
        return -1;
    if (UTILS_CHECK(name != NULL))
        return -2;
    const char *p = path + strlen(path);
    while (p != path)
    {
        if (*(--p) == '/')
            break;
    }
    if (UTILS_CHECK(p != path))
        return -3;
    p++;
    if (UTILS_CHECK(strlen(p) != 0))
        return -4;
    strcpy(name, p);
    return 0;
}

static int utils_explorer_back_path(char *name, const char *path)
{
    if (UTILS_CHECK(path != NULL))
        return -1;
    if (UTILS_CHECK(name != NULL))
        return -2;
    const char *p = path + strlen(path) - 1;
    while (p != path)
    {
        if (*(--p) == '/')
            break;
    }
    // p++;
    strncpy(name, path, p - path);
    *(name + (p - path)) = '\0';
    return 0;
}

static int utils_explorer_splice_path(const char *base, const char *name, char *result)
{
    if (strcmp(name, "..") == 0)
    {
        utils_explorer_back_path(result, base);
        return -1;
    }
    else if (strcmp(name, ".") == 0 || strcmp(name, "") == 0)
    {
        strncpy(result, base, PATH_MAX_LEN);
        return 0;
    }
    else
    {
        snprintf(result, PATH_MAX_LEN, "%s/%s", base, name);
        return 1;
    }
    return 0;
}

static int utils_explorer_update_items(explorer_t *explorer)
{
    DIR *dirp = NULL;
    struct dirent *dp = NULL;
    struct stat st;
    common_header_t header;
    int item_index = 0;
    dirp = opendir(explorer->current_path);
    if (UTILS_CHECK_ERRNO(dirp != NULL))
        return -1;

    explorer->total_number = 0;
    explorer->dir_number = 0;

    if (explorer->item_array)
    {
        free(explorer->item_array);
        explorer->item_array = NULL;
    }
    // 统计目录和总入口数量
    explorer_item_t item;
    while ((dp = readdir(dirp)) != NULL)
    {
        snprintf(item.path, PATH_MAX_LEN, "%s/%s", explorer->current_path, dp->d_name);
        // stat(item.path, &st);
        // if (S_ISDIR(st.st_mode))
        if (dp->d_type == DT_DIR)
            explorer->dir_number++;
        explorer->total_number++;
    }

    seekdir(dirp, 0);

    // 入口数量限制
    if (explorer->dir_number > EXPLORER_ITEM_ARRAY_MAX_SIZE)
        explorer->dir_number = EXPLORER_ITEM_ARRAY_MAX_SIZE;
    if (explorer->total_number > EXPLORER_ITEM_ARRAY_MAX_SIZE)
        explorer->total_number = EXPLORER_ITEM_ARRAY_MAX_SIZE;

    explorer->item_array = (explorer_item_t *)malloc(explorer->total_number * sizeof(explorer_item_t));
    if (!explorer->item_array)
    {
        explorer->dir_number = 0;
        explorer->total_number = 0;
        if (explorer->item_callback)
            explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_START);
        if (explorer->item_callback)
            explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_END);
        closedir(dirp);
        return -2;
    }

    int index = 0;
    bool save = 0;

    if (explorer->item_callback)
        explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_START);

    while ((dp = readdir(dirp)) != NULL)
    {
        strncpy(explorer->item_array[index].name, dp->d_name, NAME_MAX_LEN);
        snprintf(explorer->item_array[index].path, PATH_MAX_LEN, "%s/%s", explorer->current_path, dp->d_name);
        stat(explorer->item_array[index].path, &st);

        // if (S_ISDIR(st.st_mode))
        if (dp->d_type == DT_DIR)
        {
            explorer->item_array[index].item_mtime = st.st_mtime;
            explorer->item_array[index].is_dir = 1;
            // printf("%s - %ld - %s\n",explorer->item_array[index].path,explorer->item_array[index].item_mtime,ctime(&(explorer->item_array[index].item_mtime)));
        }
        else
        {
            explorer->item_array[index].item_mtime = st.st_mtime;
            explorer->item_array[index].is_dir = 0;
            // printf("%s - %ld - %s\n",explorer->item_array[index].path,explorer->item_array[index].item_mtime,ctime(&(explorer->item_array[index].item_mtime)));
        }

        if (explorer->item_array[index].is_dir == 0)
        {
            if (utils_get_header(&header, explorer->item_array[index].path) == 0)
                explorer->item_array[index].magic = header.magic;
            else
                LOG_I("utils_get_header failed path %s\n", explorer->item_array[index].path);
        }

        // 回调判断是否保存该项
        if (explorer->item_callback)
            save = explorer->item_callback(explorer, &explorer->item_array[index], EXPLORER_ITEM_CALLBACK_STATUS_CONTINUE);
        if (save)
        {
            if (++index == explorer->total_number)
                break;
        }
    }

    if (explorer->item_callback)
        explorer->item_callback(explorer, NULL, EXPLORER_ITEM_CALLBACK_STATUS_END);

    // 调用回调列表更新回调函数
    if (explorer->update_callback)
        explorer->update_callback(explorer);
    closedir(dirp);
}

// 带校验进度的md5计算
static int md5_progress(const char *filepath, uint8_t *digest, uint64_t size, explorer_t *explorer)
{
    FILE *fp = fopen(filepath, "rb+");
    if (fp == NULL)
    {
        LOG_E("fopen file %s failed\n",filepath);
        return -1;
    }
    HL_MD5_CTX_S ctx;
    char buf[4096] = {0};
    ssize_t len;
    uint64_t verify_offset = 0;
    uint64_t total_size = 0;
    int progress = 0;

    hl_md5_init(&ctx);
    memset(digest, 0, 16);
    while ((len = fread(buf, 1, sizeof(buf), fp)) > 0)
    {
        verify_offset += len;
        hl_md5_update(&ctx, buf, len);

        int new_progress = size > 0 ? verify_offset * 100 / size : 0;
        if (abs(progress - new_progress) > 2)
        {
            // 回调
            if (explorer->operation_callback)
                explorer->operation_callback(explorer, size, verify_offset, EXPLORER_OPERATION_VERIFYING);
            progress = new_progress;
        }
    }
    // 回调
    if (explorer->operation_callback)
        explorer->operation_callback(explorer, size, verify_offset, EXPLORER_OPERATION_VERIFYING);
        
    hl_md5_final(&ctx,digest);
    fclose(fp);
    return 0;
}

static void *utils_explore_routine(void *arg)
{
    explorer_t *explorer = arg;
    uint64_t size = 0;
    uint64_t offset = 0;
    uint64_t ticks;
    uint8_t src_md5[32] = {0};
    uint8_t dst_md5[32] = {0};

    ticks = hl_get_tick_ms();
    LOG_D("explorer->operation_state : %d\n",explorer->opertaion_state);
    if (explorer->opertaion_state == EXPLORER_OPERATION_MOVING || explorer->opertaion_state == EXPLORER_OPERATION_COPYING)
    {
        void *ctx;
        if (hl_copy_create(&ctx, explorer->operation_ctx.src_path, explorer->operation_ctx.dst_path) != 0)
        {
            if (explorer->operation_callback)
                explorer->operation_callback(explorer, size, offset, explorer->opertaion_state);    
            LOG_I("hl_copy_create failed (file:%s size:%lld offset:%lld)\n",explorer->operation_ctx.src_path,size,offset);
            explorer->opertaion_state = EXPLORER_OPERATION_IDLE;
            remove(explorer->operation_ctx.dst_path);
            return NULL;
        }
        do
        {
            explorer->operation_ctx.copy_state = hl_copy(ctx, &size, &offset);
            // 回调
            if (explorer->operation_callback)
                explorer->operation_callback(explorer, size, offset, explorer->opertaion_state);
            // 若收到停止信号...或者内存已满
            if ((sem_trywait(&explorer->operation_ctx.stop_sem) == 0) || (explorer->operation_ctx.copy_state == -1))
            {
                // 确保已完成操作的文件不会被删除
                if (offset < size)
                    remove(explorer->operation_ctx.dst_path);
                break;
            }
        } while (offset < size);
        hl_copy_destory(&ctx);

        // 标准库IO带缓存,要考虑是否马上同步内容.如果用户复制完就拔掉U盘不进行同步和umount操作肯定会丢失数据.
        utils_vfork_system("sync");
        // 完成复制
        if (offset >= size)
        {
            if (md5_progress(explorer->operation_ctx.src_path, src_md5, size, explorer) == 0 && md5_progress(explorer->operation_ctx.dst_path, dst_md5, size, explorer) == 0)
            {
#if MD5_DEBUG
                char src_md5_str[64] = {0};
                char dst_md5_str[64] = {0};
                for (int i = 0; i < 16; i++)
                {
                    sprintf(src_md5_str + i * 2, "%02x", src_md5[i]);
                    sprintf(dst_md5_str + i * 2, "%02x", dst_md5[i]);
                }
                LOG_I("%s src_md5: %s\n", explorer->operation_ctx.src_path, src_md5_str);
                LOG_I("%s dst_md5: %s\n", explorer->operation_ctx.dst_path, dst_md5_str);
#endif
                if (memcmp(src_md5, dst_md5, 16) == 0)
                {
                    if (explorer->opertaion_state == EXPLORER_OPERATION_COPYING)
                    {
                        explorer->opertaion_state = EXPLORER_OPERATION_COPY_DONE;

                        // 校验成功则将复制后的文件名进行更新（去除.tmp后缀）
                        char new_filepath[PATH_MAX_LEN + 1] = {0};
                        // 查找最后一个 '.' 的位置
                        char *dot = strrchr(explorer->operation_ctx.dst_path, '.');
                        if (dot != NULL && strcmp(dot, ".tmp") == 0)
                        {
                            // 生成新的文件名
                            strncpy(new_filepath, explorer->operation_ctx.dst_path, dot - explorer->operation_ctx.dst_path);  // 复制到新文件名
                            new_filepath[dot - explorer->operation_ctx.dst_path] = '\0';  // 结束符
                        }

                        if(strlen(new_filepath) > 0)
                        {
                            if (access(new_filepath, F_OK) == 0)
                            {
                                remove(new_filepath);
                            }

                            rename(explorer->operation_ctx.dst_path, new_filepath);
                        }
                    }
                }
                else
                {
                    explorer->opertaion_state = EXPLORER_OPERATION_COPY_FAIL;
                    LOG_E("file(%s) hl_md5 compare failed\n",explorer->operation_ctx.src_path);
                    //删除异常文件，否则后续复制都会出现校验失败情况
                    remove(explorer->operation_ctx.dst_path);
                }
            }
            else
            {
                explorer->opertaion_state = EXPLORER_OPERATION_COPY_FAIL;
                LOG_E("file(%s) hl_md5 calc failed\n",explorer->operation_ctx.src_path);
                //删除异常文件，否则后续复制都会出现校验失败情况
                remove(explorer->operation_ctx.dst_path);
            }

            if (explorer->opertaion_state == EXPLORER_OPERATION_MOVING)
            {
                // 删除源文件
                remove(explorer->operation_ctx.src_path);
                explorer->opertaion_state = EXPLORER_OPERATION_MOVE_DONE;
            }
            else if (explorer->opertaion_state == EXPLORER_OPERATION_COPYING)
                explorer->opertaion_state = EXPLORER_OPERATION_COPY_DONE;
        }
        else if (explorer->operation_ctx.copy_state == -1)
        {
            explorer->opertaion_state = EXPLORER_OPERATION_COPY_FAIL;
            LOG_E("copy failed\n");
        }
        LOG_I("copy spent ticks %llu\n", hl_get_tick_ms() - ticks);
    }
    else if (explorer->opertaion_state == EXPLORER_OPERATION_DELETE)
    {
        explorer->opertaion_state = EXPLORER_OPERATION_DELETE_DONE;
        remove(explorer->operation_ctx.dst_path);
    }

    // 通知回调函数动作已经完成
    if (explorer->operation_callback)
        explorer->operation_callback(explorer, size, offset, explorer->opertaion_state);
    explorer->opertaion_state = EXPLORER_OPERATION_IDLE;
    sem_trywait(&explorer->operation_ctx.stop_sem);
    utils_vfork_system("sync");
    return NULL;
}
