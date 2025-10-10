#ifndef APP_ITEM_H
#define APP_ITEM_H
#include "app.h"

typedef struct app_listitem_model_tag app_listitem_model_t;
typedef struct app_listitem_tag app_listitem_t;

typedef void (*app_listitem_callback_t)(widget_t **widget_list, widget_t *current_widget, void *e);
struct app_listitem_tag
{
    window_t *win;
    app_listitem_model_t *model;
};

struct app_listitem_model_tag
{
    lv_obj_t *parent;
    window_dynamic_memory_t *dm;
    int count;
    int capacity;
    app_listitem_t *item_list;
    app_listitem_callback_t callback;
    void *user_data;
};

app_listitem_model_t *app_listitem_model_create(int window_index, lv_obj_t *parent, app_listitem_callback_t callback, void *user_data);
void app_listitem_model_destory(app_listitem_model_t *model);
void app_listitem_model_push_back(app_listitem_model_t *model);
void app_listitem_model_get_visvion_range(app_listitem_model_t *model, int start_pos, int *top, int *bottom);
lv_obj_t *app_listitem_model_get_parent(app_listitem_model_t *model);
int app_listitem_model_count(app_listitem_model_t *model);
app_listitem_t *app_listitem_model_get_item(app_listitem_model_t *model, int index);
int app_listitem_model_get_item_index(app_listitem_model_t *model, app_listitem_t *item);
#endif