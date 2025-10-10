#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#include "curl/curl.h"

#include "hl_net_tool.h"
#include "hl_assert.h"
#include "hl_common.h"
#include "hl_tpool.h"
#include "hl_net.h"
#define VERBOSE 1
#define DOWNLOAD_BUF_SIZE 10 * 1024

#define LOG_TAG "hl_net_tool"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

typedef struct
{
    uint8_t *base;
    size_t offset;
    size_t length;
} curl_mem_ctx_t;

typedef struct
{
    hl_tpool_thread_t thread;
    FILE *fp;
    CURLM *curlm;
    CURL *curl;
    uint64_t offset;
    uint64_t size;
    hl_curl_download_state_t state;
} curl_download_ctx_t;

static size_t curl_mem_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
static size_t curl_file_callback(void *ptr, size_t size, size_t nmemb, void *userdata);
static void curl_download_routine(hl_tpool_thread_t thread, void *args);

int hl_curl_post(const char *url, const char *request, uint32_t request_length, char *response, uint32_t response_length, uint32_t timeout)
{
    CURL *curl = curl_easy_init();
    CURLcode code;
    curl_mem_ctx_t mem_ctx = {.base = response, .offset = 0, .length = response_length};
    if (curl == NULL)
        return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_mem_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem_ctx);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_length);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    // curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);

#if VERBOSE
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    LOG_I("request:%s\n", request);
#endif

    if ((code = curl_easy_perform(curl) != CURLE_OK))
    {
        LOG_I("hl_curl_post failed: %d\n", code);
        curl_easy_cleanup(curl);
        return -1;
    }
    curl_easy_cleanup(curl);

#if VERBOSE
    // LOG_I("response:%s\n", response);
    // printf("response:%s\n", response);
#endif
    return 0;
}
#if 0
int hl_curl_post2(const char *url, struct curl_httppost *post, char *response, uint32_t response_length, uint32_t timeout)
{
    CURL *curl = curl_easy_init();
    CURLcode code;
    curl_mem_ctx_t mem_ctx = {.base = response, .offset = 0, .length = response_length};
    if (curl == NULL)
        return -1;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_mem_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem_ctx);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, post);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);

#if VERBOSE
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

    if ((code = curl_easy_perform(curl) != CURLE_OK))
    {
        LOG_I("hl_curl_post failed: %d\n", code);
        curl_easy_cleanup(curl);
        return -1;
    }
    curl_easy_cleanup(curl);
    return 0;
}
#endif

int hl_curl_post3(const char *url, struct curl_slist *chunk, const char *request, uint32_t request_length, char *response, uint32_t response_length, uint32_t timeout)
{
    CURLcode code;
    CURLMcode mc;
    int still_running;
    curl_mem_ctx_t mem_ctx = {.base = response, .offset = 0, .length = response_length};

    CURL *curl = curl_easy_init();
    if (curl == NULL)
        return -1;

    CURLM *curlm = curl_multi_init();
    if (curlm == NULL)
    {
        LOG_I("hl_curl_post3:curl_multi_init failed\n");
        curl_easy_cleanup(curl);
        return -1;
    }

    if (curl_multi_add_handle(curlm, curl) != CURLM_OK)
    {
        LOG_I("hl_curl_post3:curl_multi_add_handle failed\n");
        curl_easy_cleanup(curl);
        curl_multi_cleanup(curlm);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_mem_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem_ctx);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_length);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    // curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);
#if VERBOSE
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    LOG_I("request:%s\n", request);
#endif

    do
    {
        CURLMcode mc = curl_multi_perform(curlm, &still_running);
        if (!mc && still_running)
            mc = curl_multi_wait(curlm, NULL, 0, 100, NULL);
        if (mc)
        {
            fprintf(stderr, "curl_multi_wait() failed, code %d.\n", (int)mc);
            break;
        }
        // } while (still_running);
    } while (still_running && hl_net_wan_is_connected());

    curl_multi_remove_handle(curlm, curl);
    curl_easy_cleanup(curl);
    curl_multi_cleanup(curlm);

    if (still_running || mc)
        return -1;

#if VERBOSE
    LOG_I("response:%s\n", response);
#endif
    return 0;
}

int hl_curl_get_download_file_size(const char *url, uint64_t *size)
{
    CURLcode code;
    CURLMcode mc;
    int still_running;

    CURL *curl = curl_easy_init();
    if (curl == NULL)
        return -1;

    CURLM *curlm = curl_multi_init();
    if (curlm == NULL)
    {
        curl_easy_cleanup(curl);
        return -1;
    }

    if (curl_multi_add_handle(curlm, curl) != CURLM_OK)
    {
        curl_easy_cleanup(curl);
        curl_multi_cleanup(curlm);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    // curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);

#if VERBOSE
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

    do
    {
        CURLMcode mc = curl_multi_perform(curlm, &still_running);
        if (!mc && still_running)
            mc = curl_multi_wait(curlm, NULL, 0, 100, NULL);
        if (mc)
        {
            LOG_I("curl_multi_wait() failed, code %d.\n", (int)mc);
            break;
        }
        // } while (still_running);
    } while (still_running && hl_net_wan_is_connected());

    if (still_running || mc)
    {
        curl_multi_remove_handle(curlm, curl);
        curl_easy_cleanup(curl);
        curl_multi_cleanup(curlm);
        LOG_I("hl_curl_get_download_file_size failed: %d %d\n", still_running, code);
        return -1;
    }

    double cl;
    if ((code = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl)) != CURLE_OK)
    {
        LOG_I("hl_curl_get_download_file_size curl_easy_getinfo failed: %d\n", code);
        curl_multi_remove_handle(curlm, curl);
        curl_easy_cleanup(curl);
        curl_multi_cleanup(curlm);
        return -1;
    }
    LOG_I("hl_curl_get_download_file_size cl %f\t%ld\n", cl, (uint64_t)cl);
    curl_multi_remove_handle(curlm, curl);
    curl_easy_cleanup(curl);
    curl_multi_cleanup(curlm);
    *size = (uint64_t)cl;
    return 0;
}

static size_t download_file_header_callback(char *buffer, size_t size,
                                            size_t nitems, void *userdata)
{
    /* received header is nitems * size long in 'buffer' NOT ZERO TERMINATED */
    /* 'userdata' is set with CURLOPT_HEADERDATA */

    char *key = strstr(buffer, "filename=");
    char *p;
    if (key != NULL)
    {
        key += sizeof("filename=\"") - 1;
        p = key;
        while (*p != '\0' && *p != '\"')
            p++;

        if (key != p)
        {
            CURL *curl = curl_easy_init();
            int decodelen;
            char *decoded = curl_easy_unescape(curl, key, p - key, &decodelen);
            strcpy((char *)userdata, decoded);
            curl_free(decoded);
            curl_easy_cleanup(curl);
        }
    }

    return nitems * size;
}

int hl_curl_get_download_file_name(const char *url, char *name, uint32_t size)
{
    LOG_I("hl_curl_get_download_file_name url %s\n", url);
    CURLcode code;
    CURLMcode mc;
    int still_running;

    CURL *curl = curl_easy_init();
    if (curl == NULL)
        return -1;

    CURLM *curlm = curl_multi_init();
    if (curlm == NULL)
    {
        curl_easy_cleanup(curl);
        return -1;
    }

    if (curl_multi_add_handle(curlm, curl) != CURLM_OK)
    {
        curl_easy_cleanup(curl);
        curl_multi_cleanup(curlm);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, download_file_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, name);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    // curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);

#if VERBOSE
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

    do
    {
        CURLMcode mc = curl_multi_perform(curlm, &still_running);
        if (!mc && still_running)
            mc = curl_multi_wait(curlm, NULL, 0, 100, NULL);
        if (mc)
        {
            LOG_I("curl_multi_wait() failed, code %d.\n", (int)mc);
            break;
        }
        // } while (still_running);
    } while (still_running && hl_net_wan_is_connected());

    curl_multi_remove_handle(curlm, curl);
    curl_easy_cleanup(curl);
    curl_multi_cleanup(curlm);

    if (still_running || mc || strlen(name) == 0)
    {
        LOG_I("hl_curl_get_download_file_name failed: %d %d %s\n", still_running, code, name);
        return -1;
    }

    return 0;
}

int hl_curl_download_task_create(hl_curl_download_ctx_t *ctx, const char *url, const char *filepath)
{
    HL_ASSERT(ctx != NULL);
    HL_ASSERT(url != NULL);
    HL_ASSERT(filepath != NULL);

    curl_download_ctx_t *c = (curl_download_ctx_t *)malloc(sizeof(curl_download_ctx_t));
    if (c == NULL)
        return -1;

    memset(c, 0, sizeof(*c));

    c->fp = fopen(filepath, "wb");
    if (c->fp == NULL)
    {
        free(c);
        return -1;
    }

    c->curl = curl_easy_init();
    if (c->curl == NULL)
    {
        fclose(c->fp);
        free(c);
        return -1;
    }

    c->curlm = curl_multi_init();
    if (c->curlm == NULL)
    {
        curl_easy_cleanup(c->curl);
        fclose(c->fp);
        free(c);
        return -1;
    }

    if (curl_multi_add_handle(c->curlm, c->curl) != CURLM_OK)
    {
        curl_easy_cleanup(c->curl);
        curl_multi_cleanup(c->curlm);
        fclose(c->fp);
        free(c);
        return -1;
    }

    curl_easy_setopt(c->curl, CURLOPT_URL, url);
    curl_easy_setopt(c->curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(c->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(c->curl, CURLOPT_WRITEFUNCTION, curl_file_callback);
    curl_easy_setopt(c->curl, CURLOPT_WRITEDATA, c);
    curl_easy_setopt(c->curl, CURLOPT_NOSIGNAL, 1);
    // curl_easy_setopt(c->curl, CURLOPT_BUFFERSIZE, DOWNLOAD_BUF_SIZE);
    // curl_easy_setopt(c->curl, CURLOPT_QUICK_EXIT, 1L);
#if VERBOSE
    curl_easy_setopt(c->curl, CURLOPT_VERBOSE, 1);
#endif

    LOG_I("ready hl_curl_get_download_file_size\n");
    if (hl_curl_get_download_file_size(url, &c->size) != 0)
    {
        curl_multi_remove_handle(c->curlm, c->curl);
        curl_easy_cleanup(c->curl);
        curl_multi_cleanup(c->curlm);
        fclose(c->fp);
        free(c);
        return -1;
    }
    LOG_I("hl_curl_get_download_file_size done -> %llu\n", c->size);
    if (c->size <= 0)
    {
        LOG_E("Error size -> %llu\n", c->size);
        return -1;
    }

    c->state = HL_CURL_DOWNLOAD_RUNNING;

    if (hl_tpool_create_thread(&c->thread, curl_download_routine, c, 0, 0, 0, 0) != 0)
    {
        curl_multi_remove_handle(c->curlm, c->curl);
        curl_easy_cleanup(c->curl);
        curl_multi_cleanup(c->curlm);
        fclose(c->fp);
        free(c);
        return -1;
    }
    hl_tpool_wait_started(c->thread, 0);
    *ctx = c;

    return 0;
}

void hl_curl_download_task_destory(hl_curl_download_ctx_t *ctx)
{
    HL_ASSERT(ctx != NULL);
    HL_ASSERT(*ctx != NULL);
    curl_download_ctx_t *c = (curl_download_ctx_t *)(*ctx);
    hl_tpool_cancel_thread(c->thread);
    hl_tpool_wait_completed(c->thread, 0);
    hl_tpool_destory_thread(&c->thread);

    LOG_I("hl_curl_download_task_destory ready clean\n");
    curl_multi_remove_handle(c->curlm, c->curl);
    curl_easy_cleanup(c->curl);
    curl_multi_cleanup(c->curlm);
    fclose(c->fp);

    free(c);
    *ctx = NULL;
}

hl_curl_download_state_t hl_curl_download_get_state(hl_curl_download_ctx_t ctx, uint64_t *offset, uint64_t *size)
{
    HL_ASSERT(ctx != NULL);
    curl_download_ctx_t *c = (curl_download_ctx_t *)ctx;
    // LOG_D("hl_curl_download_get_state %d %llu %llu\n", c->state, c->offset, c->size);
    *offset = c->offset;
    *size = c->size;
    return c->state;
}

static void curl_download_routine(hl_tpool_thread_t thread, void *args)
{
    curl_download_ctx_t *c = (curl_download_ctx_t *)args;
    CURLcode code;
    CURLMcode mc;

    int still_running;
    do
    {
        if (hl_tpool_thread_test_cancel(thread))
        {
            LOG_I("curl_download_routine cancelled\n");
            break;
        }

        CURLMcode mc = curl_multi_perform(c->curlm, &still_running);
        if (!mc && still_running)
            mc = curl_multi_wait(c->curlm, NULL, 0, 100, NULL);
        if (mc)
        {
            fprintf(stderr, "curl_multi_wait() failed, code %d.\n", (int)mc);
            c->state = HL_CURL_DOWNLOAD_FAILED;
            return;
        }
        // } while (hl_tpool_thread_test_cancel(thread) == 0 && still_running);
    } while (hl_tpool_thread_test_cancel(thread) == 0 && hl_net_wan_is_connected() && still_running);

    // LOG_I("mc = %d still_running = %d cancel %d \n", mc, still_running, hl_tpool_thread_test_cancel(thread));
    LOG_I("mc = %d still_running = %d cancel %d wlan connected %d\n", mc, still_running, hl_tpool_thread_test_cancel(thread), hl_net_wan_is_connected());
    fflush(c->fp);

    if ((c->size > 0 && c->size != c->offset) || still_running == 1)
        c->state = HL_CURL_DOWNLOAD_FAILED;
    else
        c->state = HL_CURL_DOWNLOAD_COMPLETED;
}

static size_t curl_mem_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    curl_mem_ctx_t *ctx = (curl_mem_ctx_t *)userdata;
    size_t length = (size * nmemb) > (ctx->length - ctx->offset) ? (ctx->length - ctx->offset) : (size * nmemb);
    memcpy(ctx->base + ctx->offset, ptr, length);
    ctx->offset += length;
    return length;
}

static size_t curl_file_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    curl_download_ctx_t *ctx = (curl_download_ctx_t *)userdata;
    size_t length = fwrite(ptr, size, nmemb, ctx->fp);
    ctx->offset += length;
    // LOG_I("CTX: length %d offset %llu size %llu\n", length, ctx->offset, ctx->size);
    return length;
}

int hl_curl_download_files(const char **url_list, uint32_t url_count, const char *directory)
{
    HL_ASSERT(url_list != NULL);
    HL_ASSERT(url_count != 0);
    HL_ASSERT(directory != NULL);
    CURLM *curlm = NULL;
    CURL **curls = NULL;
    FILE **fps = NULL;
    char path[PATH_MAX];
    int counts = 0;

    if (access(directory, F_OK) != 0)
        mkdir(directory, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    curls = malloc(url_count * sizeof(CURL *));
    fps = malloc(url_count * sizeof(FILE *));
    if (curls == NULL || fps == NULL)
    {
        if (curls)
            free(curls);
        if (fps)
            free(fps);
        return -1;
    }
    memset(curls, 0, url_count * sizeof(CURL *));
    memset(fps, 0, url_count * sizeof(FILE *));

    curlm = curl_multi_init();
    if (curlm == NULL)
        goto Free;

    for (int i = 0; i < url_count; i++)
    {
        curls[i] = curl_easy_init();
        if (curls[i] == NULL)
            goto Free;
        snprintf(path, sizeof(path), "%s/%s", directory, hl_get_name_from_path(url_list[i]));
        fps[i] = fopen(path, "wb+");
        if (fps[i] == NULL)
        {
            curl_easy_cleanup(curls[i]);
            goto Free;
        }
        curl_easy_setopt(curls[i], CURLOPT_URL, url_list[i]);
        curl_easy_setopt(curls[i], CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curls[i], CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curls[i], CURLOPT_WRITEDATA, fps[i]);
        curl_easy_setopt(curls[i], CURLOPT_NOSIGNAL, 1);
        // curl_easy_setopt(curls[i], CURLOPT_BUFFERSIZE, DOWNLOAD_BUF_SIZE);
        // curl_easy_setopt(curls[i], CURLOPT_QUICK_EXIT, 1L);
#if VERBOSE
        curl_easy_setopt(curls[i], CURLOPT_VERBOSE, 1);
#endif
        if (curl_multi_add_handle(curlm, curls[i]) != CURLM_OK)
        {
            fclose(fps[i]);
            curl_easy_cleanup(curls[i]);
            goto Free;
        }
        counts++;
    }

    CURLcode code;
    CURLMcode mc;
    int still_running;
    do
    {
        CURLMcode mc = curl_multi_perform(curlm, &still_running);
        if (!mc && still_running)
            mc = curl_multi_wait(curlm, NULL, 0, 1000, NULL);
        if (mc)
        {
            fprintf(stderr, "curl_multi_wait() failed, code %d.\n", (int)mc);
            break;
        }
        // } while (still_running);
    } while (still_running && hl_net_wan_is_connected());

    for (int i = 0; i < url_count; i++)
    {
        curl_multi_remove_handle(curlm, curls[i]);
        curl_easy_cleanup(curls[i]);
        fclose(fps[i]);
    }
    curl_multi_cleanup(curlm);
    free(curls);
    free(fps);
    if (still_running || mc)
        return -1;

    return 0;

Free:
    for (int i = 0; i < counts; i++)
    {
        curl_multi_remove_handle(curlm, curls[i]);
        curl_easy_cleanup(curls[i]);
        fclose(fps[i]);
    }
    if (curlm)
        curl_multi_cleanup(curlm);
    free(curls);
    free(fps);
    return -1;
}
int hl_curl_get(const char *url, char *response, uint32_t response_length, uint32_t timeout)
{
    CURLcode code;
    CURLMcode mc;
    int still_running;
    curl_mem_ctx_t mem_ctx = {.base = response, .offset = 0, .length = response_length};
    int timeout_enable = 0;
    uint64_t curr_tick = utils_get_current_tick();
    uint64_t timo_tick = curr_tick + (uint64_t)(timeout * 1000);
    if (curr_tick == timo_tick)
        timeout_enable = 0;
    else
        timeout_enable = 1;
    CURL *curl = curl_easy_init();
    if (curl == NULL)
        return -1;

    CURLM *curlm = curl_multi_init();
    if (curlm == NULL)
    {
        curl_easy_cleanup(curl);
        return -1;
    }

    if (curl_multi_add_handle(curlm, curl) != CURLM_OK)
    {
        curl_easy_cleanup(curl);
        curl_multi_cleanup(curlm);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_mem_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem_ctx);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_QUICK_EXIT, 1L);

#if VERBOSE
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif

    do
    {
        CURLMcode mc = curl_multi_perform(curlm, &still_running);
        if (!mc && still_running)
            mc = curl_multi_wait(curlm, NULL, 0, 100, NULL);
        if (mc)
        {
            LOG_I("curl_multi_wait() failed, code %d.\n", (int)mc);
            break;
        }
        if (timeout_enable == 1)
        {
            if (utils_get_current_tick() > timo_tick)
            {
                LOG_D("timeout\n");
                break;
            }
        }
    } while (still_running && hl_net_wan_is_connected());

    curl_multi_remove_handle(curlm, curl);
    curl_easy_cleanup(curl);
    curl_multi_cleanup(curlm);

    if (still_running || mc)
        return -1;

#if VERBOSE
        LOG_D("response:%s\n", response);
#endif
    return 0;
}
