#ifndef HL_NET_TOOL_H
#define HL_NET_TOOL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include "curl.h"

    typedef void *hl_curl_download_ctx_t;
    typedef enum
    {
        HL_CURL_DOWNLOAD_RUNNING,
        HL_CURL_DOWNLOAD_COMPLETED,
        HL_CURL_DOWNLOAD_FAILED,
    } hl_curl_download_state_t;

    int hl_curl_post(const char *url, const char *request, uint32_t request_length, char *response, uint32_t response_length, uint32_t timeout);
    int hl_curl_post2(const char *url, struct curl_httppost *post, char *response, uint32_t response_length, uint32_t timeout);
    int hl_curl_post3(const char *url, struct curl_slist *chunk, const char *request, uint32_t request_length, char *response, uint32_t response_length, uint32_t timeout);

    int hl_curl_download_task_create(hl_curl_download_ctx_t *ctx, const char *url, const char *filepath);
    void hl_curl_download_task_destory(hl_curl_download_ctx_t *ctx);
    hl_curl_download_state_t hl_curl_download_get_state(hl_curl_download_ctx_t ctx, uint64_t *offset, uint64_t *size);
    int hl_curl_download_files(const char **url_list, uint32_t url_count, const char *directory);
    int hl_curl_get_download_file_size(const char *url, uint64_t *size);
    int hl_curl_get_download_file_name(const char *url, char *name, uint32_t size);
    int hl_curl_get(const char *url, char *response, uint32_t response_length, uint32_t timeout);
#ifdef __cplusplus
}
#endif

#endif
