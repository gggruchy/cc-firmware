#include <stdio.h>
#include <string.h>
#include "tr.h"

int main(int argc, char **argv)
{
    tr_info_t tr_info;
    if (argc < 3)
    {
        printf("Usage: tr -g csv bin\n");
        printf("Usage: tr -p bin\n");
    }
    if (strcmp(argv[1], "-g") == 0)
    {
        tr_init(&tr_info, argv[2], argv[3]);
        tr_handle(&tr_info);
        tr_deinit(&tr_info);
    }
    else if (strcmp(argv[1], "-p") == 0)
    {
        tr_parse(argv[2]);
    }
    return 0;
}