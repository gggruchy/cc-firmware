#include "keyboard.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define KEYBOARD_STRING_START_LENGTH 64

static bool keyboard_realloc_buffer(keyboard_t *keyboard);
static bool keyboard_push(keyboard_t *keyboard, char c);
static void keyboard_pop(keyboard_t *keyboard);

keyboard_t *keyboard_create(void)
{
    keyboard_t *keyboard = (keyboard_t *)malloc(sizeof(keyboard_t));
    if (keyboard)
    {
        memset(keyboard, 0, sizeof(keyboard_t));

        //初始化Text字段
        keyboard->text = (char *)malloc(KEYBOARD_STRING_START_LENGTH + 1);
        if (keyboard->text == NULL)
        {
            free(keyboard);
            return NULL;
        }
        memset(keyboard->text, 0, KEYBOARD_STRING_START_LENGTH + 1);
        keyboard->size = KEYBOARD_STRING_START_LENGTH + 1;
        keyboard->current_input_index = 0;
    }
    return keyboard;
}

void keyboard_destroy(keyboard_t *keyboard)
{
    //调用回调函数
    for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
    {
        if (keyboard->exit_callback[i])
            keyboard->exit_callback[i](keyboard, keyboard->text);
    }
    if (keyboard->text)
    {
        if (keyboard->text)
            free(keyboard->text);
        free(keyboard);
    }
}

bool keyboard_set_text(keyboard_t *keyboard, char *fmt, ...)
{
    int n = 0;
    int oldsize = 0;
    va_list ap;
    keyboard->current_input_index = 0;

    if (!keyboard)
        return false;

    do
    {
        oldsize = keyboard->size;
        //尝试拷贝
        va_start(ap, fmt);
        n = vsnprintf(keyboard->text, oldsize, fmt, ap);
        va_end(ap);
        //重新分配内存
        if (n >= oldsize)
        {
            if (!keyboard_realloc_buffer(keyboard))
                return false;
        }
        else if (n < 0)
            return false;
    } while (n >= oldsize);
    keyboard->current_input_index = strlen(keyboard->text);
    return true;
}

const char *keyboard_get_text(keyboard_t *keyboard)
{
    if (!keyboard)
        return NULL;
    return keyboard->text;
}

void keyboard_commit_character(keyboard_t *keyboard, char c)
{
    if (!keyboard)
        return;
    switch (c)
    {
    case '\x7f': // DELETE
        keyboard_pop(keyboard);
        break;
    default:
        keyboard_push(keyboard, c);
        break;
    }
}

void keyboard_register_accept_hook(keyboard_t *keyboard, keyboard_accept_hook_t accept_hook)
{
    if (!keyboard)
        return;
    for (int i = 0; i < KEYBOARD_HOOK_MAX_SIZE; i++)
    {
        if (keyboard->accept_hook[i] == NULL)
        {
            keyboard->accept_hook[i] = accept_hook;
            break;
        }
    }
}

void keyboard_unregister_accept_hook(keyboard_t *keyboard, keyboard_accept_hook_t accept_hook)
{
    if (!keyboard)
        return;
    for (int i = 0; i < KEYBOARD_HOOK_MAX_SIZE; i++)
    {
        if (keyboard->accept_hook[i] == accept_hook)
        {
            keyboard->accept_hook[i] = NULL;
            break;
        }
    }
}

void keyboard_register_edited_callback(keyboard_t *keyboard, keyboard_edited_callback_t edited_callback)
{
    if (!keyboard)
        return;
    for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
    {
        if (keyboard->edited_callback[i] == NULL)
        {
            keyboard->edited_callback[i] = edited_callback;
            break;
        }
    }
}

void keyboard_unregister_edited_callback(keyboard_t *keyboard, keyboard_edited_callback_t edited_callback)
{
    if (!keyboard)
        return;
    for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
    {
        if (keyboard->edited_callback[i] == edited_callback)
        {
            keyboard->edited_callback[i] = NULL;
            break;
        }
    }
}

void keyboard_register_exit_callback(keyboard_t *keyboard, keyboard_exit_callback_t exit_callback)
{
    if (!keyboard)
        return;
    for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
    {
        if (keyboard->exit_callback[i] == NULL)
        {
            keyboard->exit_callback[i] = exit_callback;
            break;
        }
    }
}

void keyboard_unregister_exit_callback(keyboard_t *keyboard, keyboard_exit_callback_t exit_callback)
{
    if (!keyboard)
        return;
    for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
    {
        if (keyboard->exit_callback[i] == exit_callback)
        {
            keyboard->exit_callback[i] = NULL;
            break;
        }
    }
}

static bool keyboard_realloc_buffer(keyboard_t *keyboard)
{
    int newsize = keyboard->size * 2;
    char *newtext = (char *)realloc(keyboard->text, newsize);
    if (newtext == NULL)
        return false;
    keyboard->text = newtext;
    keyboard->size = newsize;
    return true;
}

static void keyboard_text_insert_character(keyboard_t *keyboard, char c)
{
    int len = strlen(keyboard->text);
    if (keyboard->current_input_index >= len)
    {
        keyboard->text[len] = c;
        keyboard->text[len + 1] = '\0';
        keyboard->current_input_index++;
    }
    else
    {
        // char *text_index_temp = keyboard->text + keyboard->current_input_index;
        // char *tmp_text_buffer = (char *)malloc(sizeof(len + 1));
        // strcpy(tmp_text_buffer, text_index_temp);
        // keyboard->text[keyboard->current_input_index] = c;
        // keyboard->text[keyboard->current_input_index + 1] = '\0';
        // sprintf(keyboard->text, "%s%s", keyboard->text, tmp_text_buffer);
        // keyboard->current_input_index++;
        // free(tmp_text_buffer);
        keyboard->text[len + 1] = '\0';
        for (int i = len; i > keyboard->current_input_index; i--)
        {
            keyboard->text[i] = keyboard->text[i - 1];
        }

        keyboard->text[keyboard->current_input_index] = c;
        keyboard->current_input_index++;
    }
}

static bool keyboard_push(keyboard_t *keyboard, char c)
{
    //调用钩子函数
    for (int i = 0; i < KEYBOARD_HOOK_MAX_SIZE; i++)
    {
        if (keyboard->accept_hook[i])
        {
            bool accepted = keyboard->accept_hook[i](keyboard, c);
            if (!accepted)
                return false;
        }
    }

    int len = strlen(keyboard->text);
    if (len >= keyboard->size - 1)
    {
        if (!keyboard_realloc_buffer(keyboard))
            return false;
    }

    // keyboard->text[len] = c;
    // keyboard->text[len + 1] = '\0';
    keyboard_text_insert_character(keyboard, c);

    //调用回调函数
    for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
    {
        if (keyboard->edited_callback[i])
            keyboard->edited_callback[i](keyboard, keyboard->text);
    }
    return true;
}

static void keyboard_delete_character(keyboard_t *keyboard)
{
    char *tmp_delete_character = keyboard->text; //记录删除后位置
    char *tmp = keyboard->text;                  //记录删除前位置
    for (int i = 0; i < strlen(keyboard->text); i++)
    {
        if (i != keyboard->current_input_index - 1)
            tmp_delete_character++;
        else
            break;
    }

    tmp = tmp_delete_character;
    tmp_delete_character++;
    sprintf(tmp, "%s", tmp_delete_character); //将删除后str的向前覆盖
    if (keyboard->current_input_index > 0)
        keyboard->current_input_index--;
}

static void keyboard_pop(keyboard_t *keyboard)
{
    int len = strlen(keyboard->text);
    if (len > 0 && len <= keyboard->size - 1)
    {
        keyboard_delete_character(keyboard);
        //调用回调函数
        for (int i = 0; i < KEYBOARD_CALLBACK_MAX_SIZE; i++)
        {
            if (keyboard->edited_callback[i])
                keyboard->edited_callback[i](keyboard, keyboard->text);
        }
    }
}
