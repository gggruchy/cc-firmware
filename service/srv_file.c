// #include "srv_file.h"
// #include "hl_disk.h"
// #include "simplebus.h"
// #include "pthread.h"

// #include <unistd.h>
// #include "utils/list.h"
// typedef struct
// {
//     char entry[1024];
//     srv_file_sort_type_t sort_type;
//     list_node_t node;
// } entry_t;

// static list_head_t entry_list;
// static pthread_mutex_t entry_list_lock;

// static void srv_file_service_callback(const char *name, void *context, int request_id, void *args, void *response);

// void srv_file_init(void)
// {
//     list_head_init(&entry_list);
//     pthread_mutex_init(&entry_list_lock, NULL);
//     simple_bus_register_service("srv_file", NULL, srv_file_service_callback);
// }

// static void srv_file_service_callback(const char *name, void *context, int request_id, void *args, void *response)
// {
//     switch (request_id)
//     {
//     case SRV_FILE_CREATE_ENTRY:
//     {
//         srv_file_req_create_entry_t *req = (srv_file_req_create_entry_t *)args;
//         srv_file_res_set_entry_t *res = (srv_file_res_set_entry_t *)response;
//         if (access(req->entry, F_OK) != 0)
//         {
//             res->ret = SRV_FILE_RET_ERROR;
//             break;
//         }

//         entry_t *entry = (entry_t *)malloc(sizeof(entry_t));
//         if (!entry)
//         {
//             res->ret = SRV_FILE_RET_ERROR;
//             break;
//         }

//         strncpy(entry->entry, req->entry, sizeof(entry->entry));
//         entry->sort_type = req->sort_type;
//         list_head_init(&entry->node);

//         pthread_mutex_lock(&entry_list_lock);
//         list_insert_head(&entry_list, &entry->node);
//         pthread_mutex_unlock(&entry_list_lock);
//     }
//     break;
//     case SRV_FILE_DESTORY_ENTRY:
//     {
//         srv_file_req_destory_entry_t *req = (srv_file_req_destory_entry_t *)args;
//         srv_file_res_set_entry_t *res = (srv_file_res_set_entry_t *)response;
//         entry_t *n;
//         entry_t *pos;
//         pthread_mutex_lock(&entry_list_lock);
//         list_for_each_entry_safe(pos, n, &entry_list, entry_t, node)
//         {
//             if (strncmp(pos->entry, req->entry, sizeof(pos->entry)) == 0)
//             {
//                 list_remove(&pos->node);
//                 free(pos);
//             }
//         }
//         pthread_mutex_unlock(&entry_list_lock);
//     }
//     break;
//     }
// }
