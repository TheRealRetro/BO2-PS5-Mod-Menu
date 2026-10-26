/* Runs the injected hook against a fake DB_FindXAssetHeader with the same
   20-byte prologue, with code and data in separate allocations as in the
   game, to check matching, pass-through and arguments. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

extern const uint8_t deadops_hook_begin[], deadops_hook_name_imm[];
extern const uint8_t deadops_hook_tree_imm[], deadops_hook_ret_imm[], deadops_hook_end[];
extern uint8_t fake_db_find[];   /* returns (type << 32) | (dx << 16) | cx */

typedef void *(*db_find_t)(int, const char *, int, int);

int main(void)
{
    size_t len = (size_t)(deadops_hook_end - deadops_hook_begin);
    uint8_t *code = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint8_t *data = mmap(NULL, 0x4000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint64_t tree = (uint64_t)data, name = (uint64_t)data + 0x20;
    uint64_t ret = (uint64_t)(fake_db_find + 20);
    strcpy((char *)data + 0x20, "maps/mp/gametypes_zm/_clientids.gsc");
    memcpy(code, deadops_hook_begin, len);
    memcpy(code + (deadops_hook_name_imm - deadops_hook_begin), &name, 8);
    memcpy(code + (deadops_hook_tree_imm - deadops_hook_begin), &tree, 8);
    memcpy(code + (deadops_hook_ret_imm - deadops_hook_begin), &ret, 8);
    mprotect(code, 0x4000, PROT_EXEC);   /* execute-only where the CPU allows */

    db_find_t hook = (db_find_t)code;
    int fails = 0;
#define CHECK(desc, cond) do { int ok_ = (cond); printf("%-52s %s\n", desc, ok_ ? "ok" : "FAIL"); fails += !ok_; } while (0)

    CHECK("exact script name -> our tree",
          (uint64_t)hook(0x31, "maps/mp/gametypes_zm/_clientids.gsc", 1, -1) == tree);
    CHECK("upper case + backslashes -> our tree",
          (uint64_t)hook(0x31, "MAPS\\MP\\GAMETYPES_ZM\\_ClientIds.GSC", 1, -1) == tree);
    uint64_t r = (uint64_t)hook(0x31, "maps/mp/gametypes_zm/_zm.gsc", 1, 0x1234);
    CHECK("other script -> original, args intact", r == ((0x31ULL << 32) | (1 << 16) | 0x1234));
    r = (uint64_t)hook(0x31, "maps/mp/gametypes_zm/_clientids.gs", 0, 0x55);
    CHECK("prefix of name -> original", r == ((0x31ULL << 32) | 0x55));
    r = (uint64_t)hook(0x31, "maps/mp/gametypes_zm/_clientids.gscx", 0, 0x66);
    CHECK("longer name -> original", r == ((0x31ULL << 32) | 0x66));
    r = (uint64_t)hook(0x29, "maps/mp/gametypes_zm/_clientids.gsc", 1, 7);
    CHECK("same name, rawfile type -> original", r == ((0x29ULL << 32) | (1 << 16) | 7));
    return fails != 0;
}
