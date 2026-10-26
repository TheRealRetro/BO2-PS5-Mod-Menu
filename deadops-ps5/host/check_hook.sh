#!/bin/bash
# Runs the hook tests and checks the hook never reads its own (execute-only)
# memory: no RIP-relative operands and no relocations.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
cd "$here"
clang-18 -g -o hook_test hook_test.c fake_db.S ../payload/hook.S -Wl,-z,noexecstack
./hook_test
clang-18 -c ../payload/hook.S -o obj/hook_check.o
if llvm-objdump-18 -d -M intel --section=.rodata.deadops_hook -D obj/hook_check.o | grep -q 'rip'; then
    echo "FAIL: hook has RIP-relative operands"; exit 1
fi
if [ -n "$(llvm-objdump-18 -r obj/hook_check.o | grep -E '^[0-9a-f]{16}')" ]; then
    echo "FAIL: hook has relocations"; exit 1
fi
echo "hook reads no memory of its own: ok"
