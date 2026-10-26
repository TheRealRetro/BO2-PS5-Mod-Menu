#include "headers.hpp"

declareDetour(DB_FindXAssetHeader)

#define MAX_SCRIPT_SIZE (16 * 1024 * 1024)

struct scriptOverride {
    const char* name;
    void* asset;
    int64_t size;
    int64_t mtimeSec;
    int64_t mtimeNsec;
};

static ScePthreadMutex g_scriptLock;
static scriptOverride g_scriptOverrides[128];
static int g_scriptOverrideCount = 0;

static bool isScriptName(const char* name) {
    if (!name)
        return false;

    const size_t len = strlen(name);

    return len > 4 &&
        (!strcasecmp(name + len - 4, ".gsc") || !strcasecmp(name + len - 4, ".csc"));
}

static bool scriptPath(char* out, size_t cap, const char* sub, const char* name) {
    const int n = snprintf(out, cap, "%s/%s", g_game->folder, sub);
    if (n <= 0 || (size_t)n >= cap)
        return false;

    size_t o = (size_t)n;

    for (const char* p = name; *p; ++p) {
        if (o + 1 >= cap)
            return false;

        char c = *p;

        if (c == '\\')
            c = '/';
        else if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';

        out[o++] = c;
    }

    out[o] = '\0';

    return strstr(out + n, "..") == nullptr;
}

static bool scriptFileExists(char* path, size_t cap, const char* file, SceKernelStat* st) {
    return scriptPath(path, cap, "", file) &&
        sceKernelStat(path, st) >= 0 && !SCE_KERNEL_S_ISDIR(st->st_mode);
}

static bool findScriptOverride(const char* tag, const char* name, char* path, size_t cap, SceKernelStat* st) {
    const bool entryScript =
        g_game->script && !strcasecmp(name, g_game->script);

    bool found = scriptFileExists(path, cap, name, st);

    if (!found && entryScript) {
        const char* file = strrchr(name, '/');
        found = scriptFileExists(path, cap, file ? file + 1 : name, st);
    }

    if (!found && entryScript)
        dbgPrint("[%s] %s requested, no replacement in %s", tag, name, g_game->folder);

    return found;
}

static void* cachedScript(const char* name, const SceKernelStat& st, scriptOverride** slot) {
    for (int i = 0; i < g_scriptOverrideCount; ++i) {
        scriptOverride* entry = &g_scriptOverrides[i];

        if (strcmp(entry->name, name))
            continue;

        if (entry->size == st.st_size &&
            entry->mtimeSec == st.st_mtim.tv_sec && entry->mtimeNsec == st.st_mtim.tv_nsec)
            return entry->asset;

        *slot = entry;
        return nullptr;
    }

    const int capacity = (int)(sizeof(g_scriptOverrides) / sizeof(g_scriptOverrides[0]));

    *slot = g_scriptOverrideCount < capacity ? &g_scriptOverrides[g_scriptOverrideCount] : nullptr;
    return nullptr;
}

static void cacheScript(scriptOverride* slot, const char* name, void* asset, const SceKernelStat& st) {
    if (slot == &g_scriptOverrides[g_scriptOverrideCount])
        ++g_scriptOverrideCount;

    slot->name = name;
    slot->asset = asset;
    slot->size = st.st_size;
    slot->mtimeSec = st.st_mtim.tv_sec;
    slot->mtimeNsec = st.st_mtim.tv_nsec;
}

static rawFile* t5LoadScript(const char* name) {
    char path[512];
    SceKernelStat st;

    if (!findScriptOverride("T5", name, path, sizeof(path), &st))
        return nullptr;

    scriptOverride* slot = nullptr;

    if (void* cached = cachedScript(name, st, &slot))
        return (rawFile*)cached;

    if (!slot) {
        dbgPrint("[T5] load skipped %s: override table full", name);
        return nullptr;
    }

    size_t textLen = 0;
    char* text = readWholeFile(path, &textLen, MAX_SCRIPT_SIZE - 1);
    if (!text) {
        dbgPrint("[T5] load FAILED %s: could not read %s", name, path);
        return nullptr;
    }

    const char* src = text;
    if (textLen >= 3 && (uint8_t)src[0] == 0xEF && (uint8_t)src[1] == 0xBB && (uint8_t)src[2] == 0xBF) {
        src += 3;
        textLen -= 3;
    }

    const size_t inflated = textLen + 1;
    const size_t nameLen = strlen(name);

    rawFile* rawfile = (rawFile*)malloc(
        sizeof(rawFile) + sizeof(rawFileData) + zlibStoredSize(inflated) + nameLen + 1);
    if (!rawfile) {
        free(text);
        return nullptr;
    }

    rawFileData* header = (rawFileData*)(rawfile + 1);
    uint8_t* stream = (uint8_t*)(header + 1);

    const size_t streamLen = zlibStore(stream, (const uint8_t*)src, inflated);

    char* nameCopy = (char*)stream + streamLen;
    memcpy(nameCopy, name, nameLen + 1);

    header->inflatedSize = (int32_t)inflated;
    header->compressedSize = (int32_t)streamLen;

    rawfile->name = nameCopy;
    rawfile->len = (int32_t)(sizeof(rawFileData) + streamLen);
    rawfile->buffer = (const char*)header;

    free(text);

    cacheScript(slot, rawfile->name, rawfile, st);

    dbgPrint("[T5] loaded %s from %s (%zu bytes)", name, path, textLen);

    return rawfile;
}

static scriptParseTree* t6LoadScript(const char* name) {
    char path[512];
    SceKernelStat st;

    if (!findScriptOverride("T6", name, path, sizeof(path), &st))
        return nullptr;

    scriptOverride* slot = nullptr;

    if (void* cached = cachedScript(name, st, &slot))
        return (scriptParseTree*)cached;

    if (!slot) {
        dbgPrint("[T6] load skipped %s: override table full", name);
        return nullptr;
    }

    size_t len = 0;
    char* buffer = readWholeFile(path, &len, MAX_SCRIPT_SIZE);
    if (!buffer) {
        dbgPrint("[T6] load FAILED %s: could not read %s", name, path);
        return nullptr;
    }

    if (len < 0x40 || *(const uint64_t*)buffer != GSC_MAGIC_T6) {
        size_t objLen = 0;
        const void* obj = GSC_CompileT6(name, buffer, len, &objLen);

        if (!obj || objLen < 0x40) {
            dbgPrint("[T6] compile FAILED %s: %s", name, GSC_CompileError());
            free(buffer);
            return nullptr;
        }

        char* compiled = (char*)malloc(objLen);
        if (!compiled) {
            dbgPrint("[T6] compile FAILED %s: out of memory for %zu bytes", name, objLen);
            free(buffer);
            return nullptr;
        }

        memcpy(compiled, obj, objLen);
        free(buffer);
        buffer = compiled;
        len = objLen;

        dbgPrint("[T6] compiled %s from source (%zu bytes)", name, len);
    }

    const size_t nameLen = strlen(name);

    scriptParseTree* tree = (scriptParseTree*)malloc(sizeof(scriptParseTree) + nameLen + 1);
    if (!tree) {
        free(buffer);
        return nullptr;
    }

    char* nameCopy = (char*)(tree + 1);
    memcpy(nameCopy, name, nameLen + 1);

    tree->name = nameCopy;
    tree->len = (int32_t)len;
    tree->buffer = buffer;

    cacheScript(slot, nameCopy, tree, st);

    dbgPrint("[T6] loaded %s from %s (%zu bytes)", name, path, len);

    return tree;
}

void* DB_FindXAssetHeader_h(int type, const char* asset) {
    void* original = DB_FindXAssetHeader_s(type, asset);

    if (isT5())
    {
        if (type != ASSET_RAWFILE_T5 || !isScriptName(asset))
            return original;

        scePthreadMutexLock(&g_scriptLock);

        rawFile* custom = t5LoadScript(asset);

        scePthreadMutexUnlock(&g_scriptLock);

        return custom ? custom : original;
    }

    if (isT6())
    {
        if (type != ASSET_SCRIPTPARSETREE_T6 || !isScriptName(asset))
            return original;

        scePthreadMutexLock(&g_scriptLock);

        scriptParseTree* custom = t6LoadScript(asset);

        scePthreadMutexUnlock(&g_scriptLock);

        return custom ? custom : original;
    }

    return original;
}

void* start_thread(void*) {
    remove("/data/DeadOps/log.txt");

    getBaseAddress();

    detectAndNotifyGame();

    scePthreadMutexInit(&g_scriptLock, nullptr, "Scripts");

    if (!g_game)
        return nullptr;

    createDetour(DB_FindXAssetHeader, rebaseAddress(g_game->DB_FindXAssetHeader));

    return nullptr;
}

void* stop_thread(void*) {
    removeDetour(DB_FindXAssetHeader);

    return nullptr;
}

extern "C" {
    int module_start(size_t argc, const void* args) {
        ScePthread thread;

        scePthreadCreate(&thread, nullptr, start_thread, nullptr, "DeadOps Init");
        scePthreadJoin(thread, nullptr);

        return 0;
    }

    int module_stop(size_t argc, const void* args) {
        ScePthread thread;
        scePthreadCreate(&thread, nullptr, stop_thread, nullptr, "DeadOps Fini");
        scePthreadJoin(thread, nullptr);

        return 0;
    }
}
