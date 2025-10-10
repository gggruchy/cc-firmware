#include "app_listitem.h"
#include "app.h"

#define LOG_TAG "app_item"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

#define LIST_INITIALIZE_CAPACITY 16

// void app_listitem_model_callback(widget_t **widget_list, widget_t *widget, void *e)
// {
//     if (widget_list && widget_list[0]->win && widget_list[0]->win->user_data)
//     {
//         app_listitem_model_t *model = (app_listitem_model_t *)widget_list[0]->win->user_data;
//         app_listitem_t *item = NULL;
//         int model_size = app_listitem_model_count(model);
//         if (model->callback)
//         {
//             //从模型中查找该对象
//             for (int i = 0; i < model_size; i++)
//             {
//                 if ((item = app_listitem_model_get_item(model, i))->win == widget_list[0]->win)
//                 {
//                     model->callback(widget_list[0]->win, widget, item, e);
//                     break;
//                 }
//             }
//         }
//     }
// }

app_listitem_model_t *app_listitem_model_create(int window_index, lv_obj_t *parent, app_listitem_callback_t callback, void *user_data)
{
#if 0
    app_listitem_model_t *model = NULL;
    if ((model = listitem_model_create()) == NULL)
    {
        LOG_E("listitem_model_create failed\n");
        return NULL;
    }
    listitem_model_info_t *info = (listitem_model_info_t *)malloc(sizeof(listitem_model_info_t));
    info->window_index = window_index;
    info->parent = parent;
    info->model = model;
    info->callback = callback;
    info->dm = window_dynamic_memory_alloc(window_index);
    model->user_data = info;
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    return model;
#endif

    LOG_I("app_listitem_model_create\n");

    app_listitem_model_t *model = NULL;
    if ((model = (app_listitem_model_t *)malloc(sizeof(app_listitem_model_t))) == NULL)
        return NULL;
    memset(model, 0, sizeof(app_listitem_model_t));
    if ((model->item_list = (app_listitem_t *)malloc(sizeof(app_listitem_t) * LIST_INITIALIZE_CAPACITY)) == NULL)
        return NULL;
    model->capacity = LIST_INITIALIZE_CAPACITY;
    model->parent = parent;
    model->dm = window_dynamic_memory_alloc(window_index);
    model->callback = callback;
    model->user_data = user_data;

    // lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);


    return model;
}

void app_listitem_model_destory(app_listitem_model_t *model)
{
    LOG_I("app_listitem_model_destory\n");
    if (!model)
        return;
    window_dynamic_memory_free(model->dm);
    free(model->item_list);
    free(model);
}

void app_listitem_model_push_back(app_listitem_model_t *model)
{
    if (!model)
        return;

    if (model->count >= model->capacity)
    {
        int capacity = model->capacity * 2;
        app_listitem_t *item_list = (app_listitem_t *)realloc(model->item_list, capacity * sizeof(app_listitem_t));
        if (!item_list)
            return;
        model->capacity = capacity;
        model->item_list = item_list;
    }

    app_listitem_t *item = &model->item_list[model->count++];

    if ((item->win = window_create_from_memory(model->dm, model->callback, model->parent, model)) == NULL)
    {
        model->count--;
        return;
    }
    lv_event_t e;
    e.code = (lv_event_code_t)LV_EVENT_CREATED;
    e.param = model->user_data;
    if (item->win->callback)
        item->win->callback(item->win->widget_list, NULL, &e);
}

lv_obj_t *app_listitem_model_get_parent(app_listitem_model_t *model)
{
    if (!model)
        return NULL;
    return model->parent;
}

int app_listitem_model_count(app_listitem_model_t *model)
{
    if (!model)
        return -1;
    return model->count;
}

app_listitem_t *app_listitem_model_get_item(app_listitem_model_t *model, int index)
{
    if (!model || index >= model->count)
        return NULL;
    return &model->item_list[index];
}

int app_listitem_model_get_item_index(app_listitem_model_t *model, app_listitem_t *item)
{
    if (!model || item >= model->item_list + model->count)
        return -1;
    return item - model->item_list;
}