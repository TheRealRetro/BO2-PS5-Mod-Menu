#!/bin/bash
# Builds gscc (DeadOps' T6 compiler as a Linux tool) for checking scripts.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
src="$here/../source/compiler"
obj="$here/obj"
mkdir -p "$obj"
CXX=${CXX:-clang++-18}
# -include cstring: the ORBIS headers pulled it in implicitly.
flags=(-std=c++20 -fexceptions -frtti -O1 -I"$src" -w -include cstring)
objs=()
for f in "$src"/*.cpp "$src"/t6/*.cpp "$src"/t6/misc/*.cpp "$src"/utils/*.cpp "$here/gscc.cpp"; do
    o="$obj/$(basename "$f" .cpp).o"
    "$CXX" "${flags[@]}" -c "$f" -o "$o" &
    objs+=("$o")
done
wait
"$CXX" -o "$here/gscc" "${objs[@]}"
echo "built $here/gscc"
