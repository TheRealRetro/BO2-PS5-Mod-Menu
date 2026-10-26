#!/bin/sh
# Compile the vendored T6 backend with the ORBIS toolchain, outside Visual
# Studio, so errors come back in seconds while the port is being fixed up.
set -u
CXX="/c/Program Files (x86)/SCE/ORBIS SDKs/12.000/host_tools/bin/orbis-clang++.exe"
HERE="$(cd "$(dirname "$0")" && pwd)"
OBJ="$HERE/obj"
mkdir -p "$OBJ"

# Matches the plugin: C++20 front end, size-optimised. Exceptions and RTTI are
# required by the backend (106 throw sites, 24 typeid/dynamic_cast).
FLAGS="-c -fPIC -std=c++20 -fexceptions -frtti -Oz -I$HERE"

fail=0
for src in "$HERE"/*.cpp "$HERE"/t6/*.cpp "$HERE"/t6/misc/*.cpp "$HERE"/utils/*.cpp; do
    name=$(basename "$src" .cpp)
    out="$OBJ/$name.o"
    if ! "$CXX" $FLAGS "$src" -o "$out" 2> "$OBJ/$name.log"; then
        echo "FAIL $name"
        head -${1:-12} "$OBJ/$name.log"
        fail=$((fail + 1))
    else
        echo "ok   $name ($(stat -c%s "$out") bytes)"
    fi
done
echo "---"
[ "$fail" -eq 0 ] && echo "all objects built" || echo "$fail file(s) failed"
exit $fail
