#!/bin/bash
# Builds deadops-ps5.elf: a ps5-payload-sdk payload (sent to the ELF loader
# like ploader) that compiles the T6 script and hooks BO2 from outside.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
sdk=${PS5_PAYLOAD_SDK:-/opt/ps5-payload-sdk}
src="$here/../source/compiler"
obj="$here/obj"
mkdir -p "$obj"
CXX="$sdk/bin/prospero-clang++"
CC="$sdk/bin/prospero-clang"
cxxflags=(-std=c++20 -fexceptions -frtti -Os -I"$src" -w -include cstring)
objs=()
for f in "$src"/*.cpp "$src"/t6/*.cpp "$src"/t6/misc/*.cpp "$src"/utils/*.cpp; do
    o="$obj/$(basename "$f" .cpp).o"
    "$CXX" "${cxxflags[@]}" -c "$f" -o "$o" &
    objs+=("$o")
done
wait
for f in "$here"/*.c; do
    o="$obj/$(basename "$f" .c).o"
    "$CC" -std=gnu11 -Wall -Werror -Os -I"$src" -c "$f" -o "$o"
    objs+=("$o")
done
"$CC" -c "$here/hook.S" -o "$obj/hook.o"
objs+=("$obj/hook.o")
"$CXX" -o "$here/deadops-ps5.elf" "${objs[@]}" -lSceNotification
echo "built $here/deadops-ps5.elf ($(stat -c%s "$here/deadops-ps5.elf") bytes)"
