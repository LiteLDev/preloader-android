#pragma once

#include <cstdint>
#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PLModInfo {
    uint32_t size;
    const char *mod_id;
    const char *display_name;
    const char *author;
    const char *version;
    const char *entry_path;
    const char *entry_file_name;
    const char *library_path;
    const char *icon_path;
    const char *manifest_path;
    const char *mod_root_path;
} PLModInfo;

typedef void (*PLModLoadFunc)(JavaVM *vm, const PLModInfo *mod_info);

#ifdef __cplusplus
} // extern "C"
#endif
