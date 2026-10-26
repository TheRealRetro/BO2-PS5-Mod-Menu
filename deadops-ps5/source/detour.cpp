#include "headers.hpp"

#define VM_PROT_ALL (PROT_READ | PROT_WRITE | PROT_EXEC)

#define DETOUR_REL32_RANGE 0x7FF00000ULL

#define DETOUR_JMP32_SIZE 5
#define DETOUR_JMP64_SIZE 14

#define DETOUR_MAP_SIZE 0x1000

typedef struct _GHSDK_DetourReloc
{
    size_t Offset;
    size_t Length;
    size_t DispOffset;
    bool HasDisp;
} detourReloc;

static size_t detourDecodeVex(uint64_t Address, bool* OutHasDisp, size_t* OutDispOffset)
{
    const uint8_t* p = (const uint8_t*)Address;
    size_t Pos = 0;
    uint32_t Map = 0;

    *OutHasDisp = false;
    *OutDispOffset = 0;

    if (p[0] == 0xC5)
    {
        Map = 1;
        Pos = 2;
    }
    else if (p[0] == 0xC4)
    {
        Map = p[1] & 0x1F;
        Pos = 3;
    }
    else if (p[0] == 0x62)
    {
        Map = p[1] & 0x07;
        Pos = 4;
    }
    else
    {
        return 0;
    }

    uint8_t Opcode = p[Pos];
    Pos++;

    if (Map == 1 && Opcode == 0x77)
        return Pos;

    uint8_t ModRM = p[Pos];
    Pos++;

    uint8_t Mod = (ModRM >> 6) & 3;
    uint8_t Rm = ModRM & 7;

    if (Mod != 3)
    {
        if (Rm == 4)
        {
            uint8_t Sib = p[Pos];
            Pos++;

            if (Mod == 0 && (Sib & 7) == 5)
                Pos += 4;
        }
        else if (Mod == 0 && Rm == 5)
        {
            *OutHasDisp = true;
            *OutDispOffset = Pos;
            Pos += 4;
        }

        if (Mod == 1)
            Pos += 1;
        else if (Mod == 2)
            Pos += 4;
    }

    if (Map == 3)
    {
        Pos += 1;
    }
    else if (Map == 1 &&
        (Opcode == 0x70 || Opcode == 0x71 || Opcode == 0x72 || Opcode == 0x73 ||
            Opcode == 0xC2 || Opcode == 0xC4 || Opcode == 0xC5 || Opcode == 0xC6))
    {
        Pos += 1;
    }

    return Pos;
}

static size_t detourAnalyze(uint64_t Address, size_t MinSize,
    detourReloc* Out, size_t Max, size_t* OutCount, bool Quiet)
{
    size_t Total = 0;

    *OutCount = 0;

    if (!Address)
        return 0;

    while (Total < MinSize)
    {
        uint64_t Current = Address + Total;

        bool HasDisp = false;
        size_t DispOffset = 0;
        size_t Length = detourDecodeVex(Current, &HasDisp, &DispOffset);

        if (Length == 0)
        {
            hde64s hs;
            Length = hde64_disasm((void*)Current, &hs);

            if (hs.flags & F_ERROR)
            {
                const uint8_t* b = (const uint8_t*)Current;

                if (!Quiet)
                {
                    dbgPrint("[Detour] decode error at 0x%llX (+0x%zX) flags 0x%X",
                        (unsigned long long)Address, Total, hs.flags);
                    dbgPrint("[Detour] bytes %02X %02X %02X %02X %02X %02X %02X %02X",
                        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
                }
                return 0;
            }

            if ((hs.flags & F_RELATIVE) && (hs.flags & F_IMM8))
            {
                if (!Quiet)
                    dbgPrint("[Detour] rel8 branch at 0x%llX (+0x%zX) cannot be relocated",
                        (unsigned long long)Address, Total);
                return 0;
            }

            if ((hs.flags & F_RELATIVE) && (hs.flags & F_IMM32))
            {
                HasDisp = true;
                DispOffset = Length - 4;
            }
            else if ((hs.flags & F_MODRM) && hs.modrm_mod == 0 && hs.modrm_rm == 5 &&
                (hs.flags & F_DISP32))
            {
                size_t ImmSize = 0;
                if (hs.flags & F_IMM8)  ImmSize = 1;
                if (hs.flags & F_IMM16) ImmSize = 2;
                if (hs.flags & F_IMM32) ImmSize = 4;
                if (hs.flags & F_IMM64) ImmSize = 8;

                HasDisp = true;
                DispOffset = Length - ImmSize - 4;
            }
        }

        if (Total + Length > DETOUR_MAX_PATCH)
        {
            if (!Quiet)
                dbgPrint("[Detour] prologue at 0x%llX needs more than %d bytes",
                    (unsigned long long)Address, DETOUR_MAX_PATCH);
            return 0;
        }

        if (*OutCount >= Max)
        {
            if (!Quiet)
                dbgPrint("[Detour] too many instructions in prologue at 0x%llX",
                    (unsigned long long)Address);
            return 0;
        }

        detourReloc& Info = Out[*OutCount];
        Info.Offset = Total;
        Info.Length = Length;
        Info.HasDisp = HasDisp;
        Info.DispOffset = HasDisp ? (Total + DispOffset) : 0;

        (*OutCount)++;
        Total += Length;
    }

    return Total;
}

static void* detourMapNear(uint64_t Anchor, size_t Size)
{
    uint64_t Low = (Anchor > DETOUR_REL32_RANGE) ? (Anchor - DETOUR_REL32_RANGE) : 0x10000ULL;
    uint64_t High = Anchor + DETOUR_REL32_RANGE;

    for (uint64_t Step = 0x100000; Step < DETOUR_REL32_RANGE; Step += 0x100000)
    {
        uint64_t Hints[2];
        Hints[0] = (Anchor + Step) & ~0xFFFFULL;
        Hints[1] = (Anchor > Step) ? ((Anchor - Step) & ~0xFFFFULL) : 0;

        for (int i = 0; i < 2; i++)
        {
            if (Hints[i] < Low || Hints[i] >= High)
                continue;

            void* Addr = 0;
            int res = sceKernelMmap((void*)Hints[i], Size, VM_PROT_ALL, 0x1000 | 0x2, -1, 0, &Addr);

            if (res < 0 || Addr == 0)
                continue;

            if ((uint64_t)Addr >= Low && (uint64_t)Addr < High)
                return Addr;

            sceKernelMunmap(Addr, Size);
        }
    }

    return 0;
}

static void detourBuildJump(uint8_t* Out, uint64_t At, uint64_t Destination, bool Absolute)
{
    if (Absolute)
    {
        Out[0] = 0xFF;
        Out[1] = 0x25;
        Out[2] = 0x00;
        Out[3] = 0x00;
        Out[4] = 0x00;
        Out[5] = 0x00;
        memcpy(Out + 6, &Destination, sizeof(Destination));
    }
    else
    {
        int32_t Displacement = (int32_t)((int64_t)Destination -
            ((int64_t)At + DETOUR_JMP32_SIZE));

        Out[0] = 0xE9;
        memcpy(Out + 1, &Displacement, sizeof(Displacement));
    }
}

static bool detourInRel32Range(uint64_t From, uint64_t To)
{
    int64_t Displacement = (int64_t)To - (int64_t)From;
    return Displacement >= INT32_MIN && Displacement <= INT32_MAX;
}

static bool detourWriteVerified(void* Address, const uint8_t* Bytes, size_t Size)
{
    sceKernelMprotect(Address, Size, VM_PROT_ALL);
    memcpy(Address, Bytes, Size);

    if (memcmp(Address, Bytes, Size) != 0)
    {
        const uint8_t* a = (const uint8_t*)Address;

        dbgPrint("[Detour] write to 0x%llX did not stick", (unsigned long long)(uintptr_t)Address);
        dbgPrint("[Detour] wanted %02X %02X %02X %02X %02X %02X",
            Bytes[0], Bytes[1], Bytes[2], Bytes[3], Bytes[4], Bytes[5]);
        dbgPrint("[Detour] got    %02X %02X %02X %02X %02X %02X",
            a[0], a[1], a[2], a[3], a[4], a[5]);
        return false;
    }

    return true;
}

void* detourAttach(detour* This, uint64_t functionPtr, void* hookPtr, void** OutStub)
{
    if (OutStub)
        *OutStub = 0;

    if (!This || !functionPtr || !hookPtr)
    {
        dbgPrint("[Detour] attach with null argument (0x%llX -> 0x%llX)",
            (unsigned long long)functionPtr, (unsigned long long)(uintptr_t)hookPtr);
        return 0;
    }

    if (This->installed)
    {
        dbgPrint("[Detour] 0x%llX is already hooked", (unsigned long long)functionPtr);

        if (OutStub)
            *OutStub = This->stubPtr;

        return This->stubPtr;
    }

    memset(This, 0, sizeof(*This));

    {
        const uint8_t* b = (const uint8_t*)functionPtr;
        dbgPrint("[Detour] 0x%llX prologue %02X %02X %02X %02X %02X %02X %02X %02X "
            "%02X %02X %02X %02X %02X %02X",
            (unsigned long long)functionPtr,
            b[0], b[1], b[2], b[3], b[4], b[5], b[6],
            b[7], b[8], b[9], b[10], b[11], b[12], b[13]);
    }

    detourReloc Relocs[DETOUR_MAX_PATCH];
    size_t RelocCount = 0;

    bool Absolute = true;
    size_t patchSize = detourAnalyze(functionPtr, DETOUR_JMP64_SIZE,
        Relocs, DETOUR_MAX_PATCH, &RelocCount, true);

    if (patchSize < DETOUR_JMP64_SIZE)
    {
        dbgPrint("[Detour] 0x%llX cannot spare %d bytes, trying rel32",
            (unsigned long long)functionPtr, DETOUR_JMP64_SIZE);

        Absolute = false;
        patchSize = detourAnalyze(functionPtr, DETOUR_JMP32_SIZE,
            Relocs, DETOUR_MAX_PATCH, &RelocCount, false);

        if (patchSize < DETOUR_JMP32_SIZE)
            return 0;
    }

    void* mapPtr = detourMapNear(functionPtr, DETOUR_MAP_SIZE);

    if (!mapPtr)
    {
        int res = sceKernelMmap(0, DETOUR_MAP_SIZE, VM_PROT_ALL, 0x1000 | 0x2, -1, 0, &mapPtr);

        if (res < 0 || mapPtr == 0)
        {
            dbgPrint("[Detour] could not map stub for 0x%llX (0x%X)",
                (unsigned long long)functionPtr, res);
            return 0;
        }
    }

    uint8_t* Stub = (uint8_t*)mapPtr;
    uint8_t* JumpBack = Stub + patchSize;
    uint8_t* Bridge = JumpBack + DETOUR_JMP64_SIZE;

    bool DirectRel32 = detourInRel32Range(functionPtr + DETOUR_JMP32_SIZE, (uint64_t)hookPtr);
    bool BridgeReachable = detourInRel32Range(functionPtr + DETOUR_JMP32_SIZE, (uint64_t)Bridge);

    if (!Absolute && !DirectRel32 && !BridgeReachable)
    {
        dbgPrint("[Detour] no rel32 path from 0x%llX to hook or bridge",
            (unsigned long long)functionPtr);
        sceKernelMunmap(mapPtr, DETOUR_MAP_SIZE);
        return 0;
    }

    memcpy(This->original, (void*)functionPtr, patchSize);

    memcpy(Stub, (void*)functionPtr, patchSize);

    for (size_t i = 0; i < RelocCount; i++)
    {
        if (!Relocs[i].HasDisp)
            continue;

        int32_t OldDisp = 0;
        memcpy(&OldDisp, (void*)(functionPtr + Relocs[i].DispOffset), sizeof(OldDisp));

        uint64_t Target = functionPtr + Relocs[i].Offset + Relocs[i].Length + (int64_t)OldDisp;
        int64_t NewDisp = (int64_t)Target -
            (int64_t)((uint64_t)Stub + Relocs[i].Offset + Relocs[i].Length);

        if (NewDisp < INT32_MIN || NewDisp > INT32_MAX)
        {
            dbgPrint("[Detour] stub 0x%llX too far from 0x%llX to relocate +0x%zX",
                (unsigned long long)(uintptr_t)Stub, (unsigned long long)functionPtr,
                Relocs[i].Offset);
            sceKernelMunmap(mapPtr, DETOUR_MAP_SIZE);
            return 0;
        }

        int32_t Written = (int32_t)NewDisp;
        memcpy(Stub + Relocs[i].DispOffset, &Written, sizeof(Written));
    }

    uint8_t JumpBytes[DETOUR_JMP64_SIZE];

    detourBuildJump(JumpBytes, (uint64_t)JumpBack, functionPtr + patchSize, true);
    if (!detourWriteVerified(JumpBack, JumpBytes, DETOUR_JMP64_SIZE))
    {
        sceKernelMunmap(mapPtr, DETOUR_MAP_SIZE);
        return 0;
    }

    detourBuildJump(JumpBytes, (uint64_t)Bridge, (uint64_t)hookPtr, true);
    if (!detourWriteVerified(Bridge, JumpBytes, DETOUR_JMP64_SIZE))
    {
        sceKernelMunmap(mapPtr, DETOUR_MAP_SIZE);
        return 0;
    }

    uint8_t Patch[DETOUR_MAX_PATCH];
    memset(Patch, 0x90, patchSize);

    uint64_t JumpTarget = Absolute ? (uint64_t)hookPtr
        : (DirectRel32 ? (uint64_t)hookPtr : (uint64_t)Bridge);

    detourBuildJump(Patch, functionPtr, JumpTarget, Absolute);

    if (OutStub)
        *OutStub = Stub;

    if (!detourWriteVerified((void*)functionPtr, Patch, patchSize))
    {
        if (OutStub)
            *OutStub = 0;

        sceKernelMunmap(mapPtr, DETOUR_MAP_SIZE);
        return 0;
    }

    This->functionPtr = (void*)functionPtr;
    This->hookPtr = hookPtr;
    This->stubPtr = Stub;
    This->mapPtr = mapPtr;
    This->mapSize = DETOUR_MAP_SIZE;
    This->patchSize = patchSize;
    This->installed = true;

    dbgPrint("[Detour] 0x%llX patched with %s, %zu bytes, stub 0x%llX",
        (unsigned long long)functionPtr,
        Absolute ? "abs64" : (DirectRel32 ? "direct rel32" : "rel32 via bridge"),
        patchSize, (unsigned long long)(uintptr_t)Stub);

    return Stub;
}

void detourDetach(detour* This)
{
    if (!This || !This->installed)
        return;

    detourWriteVerified(This->functionPtr, This->original, This->patchSize);

    sceKernelUsleep(20000);

    sceKernelMunmap(This->mapPtr, This->mapSize);

    dbgPrint("[Detour] 0x%llX restored (%zu bytes)",
        (unsigned long long)(uintptr_t)This->functionPtr, This->patchSize);

    memset(This, 0, sizeof(*This));
}
