#!/usr/bin/env bash
# BO2 Mod Installer - native app build (Linux/WSL).
# Follows the Mario Collection / ps5-native-app-boilerplate pipeline:
# compile -> link with app CRT -> convert to an SCE executable -> fSELF sign.
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mario=${MARIO_DIR:-$HOME/ps5-native-app-boilerplate}
sdk=${APP_SDK:-$mario/.deps/native/ps5-payload-sdk}
tool=${PS5_NATIVE_TOOL:-$mario/build/host/ps5-native-tool}
clang=${PS5_CLANG:-$(command -v clang-18 || true)}

for path in "$sdk/bin/prospero-lld" "$tool" "$clang"; do
    [[ -x $path ]] || { echo "missing build input: $path" >&2; exit 2; }
done

title_id=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$root/sce_sys/param.json")

# Loader/container constants the boilerplate validated on hardware.
module_sdk=0x02000009
companion_sdk=0x08050001
fself_magic=0x1D3D154F

build="$root/build"
app="$root/dist/$title_id"
rm -rf -- "$build" "$app"
mkdir -p "$build/obj" "$app/sce_sys" "$app/sce_module" "$app/assets"

cc() {
    PS5_PAYLOAD_SDK="$sdk" PS5_CLANG="$clang" USE_CCACHE=0 \
        sh "$root/tooling/prospero-clang18" "$@"
}

echo "==> compile"
cc -std=c11 -O2 -Wall -Werror -ffunction-sections -fdata-sections \
    -c "$root/src/main.c" -o "$build/obj/main.o"
for name in app_crt app_cpp_runtime; do
    cc -std=c++20 -O2 -Wall -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections \
        -c "$root/tooling/native/$name.cpp" -o "$build/obj/$name.o"
done

echo "==> link"
"$sdk/bin/prospero-lld" -T "$root/tooling/native/ps5-pie.ld" --eh-frame-hdr \
    --version-script "$root/tooling/native/app-symbols.map" \
    -e _start -o "$build/llvm-pie.elf" \
    "$build/obj/app_crt.o" "$build/obj/app_cpp_runtime.o" "$build/obj/main.o" \
    --as-needed "$sdk"/target/lib/*.so

echo "==> convert to SCE executable"
"$tool" link --in "$build/llvm-pie.elf" --out "$build/eboot.elf" \
    --stub-dir "$sdk/target/lib" --module-sdk "$module_sdk" \
    --companion-sdk "$companion_sdk" --file-name eboot.elf

echo "==> sign"
"$tool" self --sign --in "$build/eboot.elf" --out "$app/eboot.bin" --magic "$fself_magic"

echo "==> assemble $app"
cp "$root/sce_sys/param.json" "$app/sce_sys/"
for asset in icon0.png pic0.dds pic1.dds snd0.at9; do
    [[ ! -f $root/sce_sys/$asset ]] || cp "$root/sce_sys/$asset" "$app/sce_sys/"
done
cp "$root/sce_module/libc.prx" "$app/sce_module/"
for asset in _clientids.gsc deadops-ps5.elf; do
    if [[ -f $root/assets/$asset ]]; then
        cp "$root/assets/$asset" "$app/assets/"
    else
        echo "WARNING: assets/$asset missing" >&2
    fi
done

"$tool" self --inspect --file "$app/sce_module/libc.prx"
"$tool" self --inspect --file "$app/eboot.bin"
echo "Build complete: $app"
