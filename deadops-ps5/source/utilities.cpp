#include "headers.hpp"

const gameInfo* g_game = nullptr;

uint64_t getBaseAddress()
{
    static uint64_t cached = 0;
    if (cached)
        return cached;

    SceKernelVirtualQueryInfo info;
    void* address = nullptr;

    while (sceKernelVirtualQuery(address, SCE_KERNEL_VQ_FIND_NEXT, &info, sizeof(info)) >= 0)
    {
        uintptr_t s = (uintptr_t)info.start;
        uintptr_t e = (uintptr_t)info.end;

        if (e <= s)
            break;

        address = (void*)e;

        if (info.protection == 5 && !strcmp(info.name, "executable"))
        {
            cached = s;
            return cached;
        }
    }

    return 0;
}

uintptr_t readableEnd(uintptr_t addr, size_t need)
{
    if (addr < 0x10000 || addr >= 0x0000800000000000ULL)
        return 0;
    if (addr + need < addr)
        return 0;

    SceKernelVirtualQueryInfo info;
    uintptr_t end = 0;
    uintptr_t probe = addr;

    for (int hop = 0; hop < 8; ++hop)
    {
        if (sceKernelVirtualQuery((void*)probe, 0, &info, sizeof(info)) < 0)
            break;

        if ((info.protection & 1) == 0)
            break;

        uintptr_t s = (uintptr_t)info.start;
        uintptr_t e = (uintptr_t)info.end;

        if (e <= s)
            break;
        if (probe < s || probe >= e)
            break;
        if (end && s > end)
            break;

        end = e;
        probe = e;

        if (end >= addr + need)
            break;
    }

    return end;
}

bool hasValidOffset(uint64_t address)
{
    return address && address != 0xDEADBEEF && quickPtr((uintptr_t)address);
}

bool quickPtr(uintptr_t p)
{
    return p >= 0x10000 && p < 0x0000800000000000ULL;
}

bool safeStrStr(uintptr_t addr, const char* target, size_t maxScan)
{
    if (!quickPtr(addr) || !target)
        return false;

    const size_t tlen = strlen(target);
    if (tlen == 0 || tlen > maxScan)
        return false;

    uintptr_t end = readableEnd(addr, maxScan);
    if (!end || end <= addr)
        return false;

    size_t avail = (size_t)(end - addr);
    if (avail > maxScan) avail = maxScan;
    if (avail < tlen) return false;

    const char* p = (const char*)addr;

    for (size_t i = 0; i + tlen <= avail; i++)
    {
        if (p[i] == '\0')
            return false;

        size_t j = 0;
        while (j < tlen && p[i + j] == target[j])
            j++;

        if (j == tlen)
            return true;
    }

    return false;
}

char* readWholeFile(const char* path, size_t* outSize, size_t maxSize)
{
    int fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY, 0);
    if (fd < 0)
        return nullptr;

    SceKernelStat st;
    if (sceKernelFstat(fd, &st) < 0 || st.st_size < 0 || (uint64_t)st.st_size > maxSize)
    {
        sceKernelClose(fd);
        return nullptr;
    }

    const size_t size = (size_t)st.st_size;

    char* data = (char*)malloc(size + 1);
    if (!data)
    {
        sceKernelClose(fd);
        return nullptr;
    }

    size_t got = 0;
    while (got < size)
    {
        ssize_t r = sceKernelRead(fd, data + got, size - got);
        if (r <= 0)
            break;
        got += (size_t)r;
    }

    sceKernelClose(fd);

    if (got != size)
    {
        free(data);
        return nullptr;
    }

    data[size] = '\0';
    *outSize = size;
    return data;
}

uint32_t adler32(const uint8_t* data, size_t len)
{
    uint32_t a = 1;
    uint32_t b = 0;

    while (len)
    {
        size_t chunk = len < 5552 ? len : 5552;
        len -= chunk;

        while (chunk--)
        {
            a += *data++;
            b += a;
        }

        a %= 65521;
        b %= 65521;
    }

    return (b << 16) | a;
}

size_t zlibStoredSize(size_t len)
{
    const size_t blocks = len ? (len + 0xFFFE) / 0xFFFF : 1;
    return 2 + blocks * 5 + len + 4;
}

size_t zlibStore(uint8_t* out, const uint8_t* data, size_t len)
{
    const uint32_t adler = adler32(data, len);
    uint8_t* o = out;

    *o++ = 0x78;
    *o++ = 0x01;

    do
    {
        const size_t chunk = len < 0xFFFF ? len : 0xFFFF;
        len -= chunk;

        *o++ = len ? 0x00 : 0x01;
        *o++ = (uint8_t)chunk;
        *o++ = (uint8_t)(chunk >> 8);
        *o++ = (uint8_t)~chunk;
        *o++ = (uint8_t)(~chunk >> 8);

        memcpy(o, data, chunk);
        o += chunk;
        data += chunk;
    } while (len);

    *o++ = (uint8_t)(adler >> 24);
    *o++ = (uint8_t)(adler >> 16);
    *o++ = (uint8_t)(adler >> 8);
    *o++ = (uint8_t)adler;

    return (size_t)(o - out);
}

void dbgPrint(const char* fmt, ...)
{
#ifdef _DEBUG
    char body[1024];
    va_list va;
    va_start(va, fmt);
    vsnprintf(body, sizeof(body), fmt, va);
    va_end(va);

    char msg[1088];
    int len = snprintf(msg, sizeof(msg), "[DeadOps] %s\n", body);
    if (len < 0)
        return;
    if (len >= (int)sizeof(msg))
        len = (int)sizeof(msg) - 1;

    int fd = sceKernelOpen("/data/DeadOps/log.txt", SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_APPEND, 0777);
    if (fd < 0)
        return;

    int written = 0;
    while (written < len)
    {
        int r = sceKernelWrite(fd, msg + written, len - written);
        if (r <= 0)
            break;
        written += r;
    }

    sceKernelClose(fd);
#endif
}

void detectAndNotifyGame()
{
    for (const gameInfo& entry : gameTable)
    {
        const uintptr_t address = rebaseAddress(entry.detect);

        if (hasValidOffset(address) && safeStrStr(address, "ui_multiplayer"))
        {
            g_game = &entry;
            break;
        }
    }

    if (!g_game)
    {
        dbgPrint("Unsupported title detected!");
        return;
    }

    dbgPrint("Detected game: %s", g_game->label);

    dbgPrint("[Offset Values]");
    dbgPrint("  DB_FindXAssetHeader                                : 0x%p",
        (void*)rebaseAddress(g_game->DB_FindXAssetHeader));

    dbgPrint("[DB_FindXAssetHeader Values]");
    dbgPrint("  Name       : %s", g_game->script);
    dbgPrint("  Folder     : %s", g_game->folder);
}
