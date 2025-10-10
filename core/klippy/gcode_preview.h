#ifndef _GCODE_PREVIEW_H
#define _GCODE_PREVIEW_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "print_stats_c.h"
#define THUMBNAIL_IMG "/tmp/thumbnail/thumbnail.png"
#define THUMBNAIL_DIR "/tmp/thumbnail"

    int gcode_preview(const char *file_path, char *preview_path, int need_preview, slice_param_t *slice_param, char *file_name);

#ifdef __cplusplus
}
#endif
#endif