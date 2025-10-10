#include <stdio.h>
#include <string.h>
#include "widget.h"

widget_info_t widget_info;
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: widget -g csv bin\n");
        printf("Usage: widget -p bin\n");
    }
    if (strcmp(argv[1], "-g") == 0)
    {
        widget_init(&widget_info, argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "-p") == 0)
    {
        widget_parse(argv[2]);
    }
    return 0;
}