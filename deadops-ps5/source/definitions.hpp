#pragma once

enum game {
    NONE,

    T5_CP,
    T5_MP,

    T6_SP,
    T6_ZM,
    T6_MP,
};

struct gameInfo {
    game id;
    const char* label;
    uint64_t detect;
    uint64_t DB_FindXAssetHeader;
    const char* script;
    const char* folder;
};

inline constexpr gameInfo gameTable[] = {
    { T5_CP, "T5_CP", 0x6E0556, 0x161A80, "maps/gametypes/cmp.gsc",              "/data/DeadOps/t5_cp" },
    { T5_MP, "T5_MP", 0x761B3B, 0x1CEA30, "maps/mp/gametypes/_clientids.gsc",    "/data/DeadOps/t5_mp" },

    { T6_SP, "T6_SP", 0x8152C3, 0x1A6030, "maps/_callbacksetup.gsc",             "/data/DeadOps/t6_sp" },
    { T6_ZM, "T6_ZM", 0x952084, 0x1BFAE0, "maps/mp/gametypes_zm/_clientids.gsc", "/data/DeadOps/t6_zm" },
    { T6_MP, "T6_MP", 0x955624, 0x1BE980, "maps/mp/gametypes/_clientids.gsc",    "/data/DeadOps/t6_mp" },
};

extern const gameInfo* g_game;

inline bool isT5() {
    return g_game && g_game->id <= T5_MP;
}

inline bool isT6() {
    return g_game && g_game->id >= T6_SP;
}

enum assetType {
    ASSET_RAWFILE_T5 = 0x24,
    ASSET_SCRIPTPARSETREE_T6 = 0x31,
};

struct rawFileData {
    int32_t inflatedSize;
    int32_t compressedSize;
};

struct xAsset {
    const char* name;
    int32_t len;
    const char* buffer;
};

typedef xAsset rawFile;
typedef xAsset scriptParseTree;

inline constexpr uint64_t GSC_MAGIC_T6 = 0x06000A0D43534780ull;

using DB_FindXAssetHeader_t = void* (*)(int, const char*);
inline DB_FindXAssetHeader_t DB_FindXAssetHeader_s;
