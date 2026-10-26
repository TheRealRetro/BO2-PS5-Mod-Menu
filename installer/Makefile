# BO2 Mod Menu Installer - PS5 Native App
# Build: Linux/WSL. The real steps live in tools/build.sh.
#
# Usage:
#   make            Build dist/PPSA99002/
#   make clean      Remove build/ and dist/
#
# Overrides: MARIO_DIR (source of the SDK and ps5-native-tool),
#            APP_SDK, PS5_NATIVE_TOOL, PS5_CLANG, PS5_PAYLOAD_SDK
# Set them on the command line or in installer/.env (not committed).

-include .env
export MARIO_DIR APP_SDK PS5_NATIVE_TOOL PS5_CLANG PS5_PAYLOAD_SDK

all:
	bash tools/build.sh

# DeadOps-PS5 payload (../deadops-ps5): compiles the script and hooks the PS5
# zombies build. Output goes into assets/ for the app bundle.
deadops:
	bash ../deadops-ps5/payload/build.sh
	cp ../deadops-ps5/payload/deadops-ps5.elf assets/

clean:
	rm -rf build dist

.PHONY: all deadops clean
