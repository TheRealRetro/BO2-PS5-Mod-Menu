#pragma once

#define rebaseAddress(base) ((uintptr_t)(base) + getBaseAddress())

uint64_t getBaseAddress();
uintptr_t readableEnd(uintptr_t addr, size_t need);
bool hasValidOffset(uint64_t address);
bool quickPtr(uintptr_t p);
bool safeStrStr(uintptr_t addr, const char* target, size_t maxScan = 64);
char* readWholeFile(const char* path, size_t* outSize, size_t maxSize);
uint32_t adler32(const uint8_t* data, size_t len);
size_t zlibStoredSize(size_t len);
size_t zlibStore(uint8_t* out, const uint8_t* data, size_t len);
void dbgPrint(const char* fmt, ...);
void detectAndNotifyGame();
