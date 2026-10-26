#pragma once

#define DETOUR_MAX_PATCH 32

typedef struct _GHSDK_Detour
{
    void* functionPtr;
    void* hookPtr;
    void* stubPtr;
    void* mapPtr;
    size_t mapSize;
    size_t patchSize;
    uint8_t original[DETOUR_MAX_PATCH];
    bool installed;
} detour;

void* detourAttach(detour* This, uint64_t functionPtr, void* hookPtr, void** OutStub);
void detourDetach(detour* This);

#define declareDetour(name) detour name##_d{};

#define createDetour(name, offset)                                          \
    do {                                                                    \
        if (hasValidOffset(offset))                                         \
        {                                                                   \
            detourAttach(&name##_d, (uint64_t)(offset),                    \
                (void*)name##_h, (void**)&name##_s);                        \
                                                                            \
            if (name##_s == 0)                                              \
            {                                                               \
                dbgPrint("[Detour] FAILED %s at 0x%llX",                    \
                    #name, (unsigned long long)(offset));                   \
            }                                                               \
            else                                                            \
            {                                                               \
                dbgPrint("[Detour] hooked %s at 0x%llX -> 0x%llX "          \
                    "(stub 0x%llX, %zu bytes patched)",                     \
                    #name,                                                  \
                    (unsigned long long)(offset),                           \
                    (unsigned long long)(uintptr_t)name##_h,                \
                    (unsigned long long)(uintptr_t)name##_s,                \
                    name##_d.patchSize);                                    \
            }                                                               \
        }                                                                   \
        else                                                                \
        {                                                                   \
            dbgPrint("[Detour] skipped %s - invalid offset 0x%llX",         \
                #name, (unsigned long long)(offset));                       \
        }                                                                   \
    } while (0)

#define removeDetour(name)                                                  \
    do {                                                                    \
        if (name##_d.installed)                                             \
        {                                                                   \
            detourDetach(&name##_d);                                       \
            name##_s = 0;                                                   \
        }                                                                   \
    } while (0)
