#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const void* GSC_CompileT6(const char* name, const void* src, size_t srcLen, size_t* outLen);

const char* GSC_CompileError(void);

void GSC_CompileShutdown(void);

#ifdef __cplusplus
}
#endif
