# BO2 Mod Menu Installer - Build Guide

## Prerequisites (Linux/WSL)

- `clang-18`, `python3`
- [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk), for the payload (default `/opt/ps5-payload-sdk`)
- A checkout of [ps5-native-app-boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  (or a project built from it) that supplies:
  - the ps5-payload-sdk at `.deps/native/ps5-payload-sdk`
  - `build/host/ps5-native-tool` (converts and signs the eboot)

`tools/build.sh` defaults to `~/ps5-native-app-boilerplate`. Point it at your checkout with
`MARIO_DIR=...`, or set `APP_SDK`, `PS5_NATIVE_TOOL` and `PS5_CLANG` individually.
`make deadops` uses `PS5_PAYLOAD_SDK` (default `/opt/ps5-payload-sdk`).
To keep these out of the command line, put them in `installer/.env` (git-ignored), for example:

```make
MARIO_DIR=/path/to/your/boilerplate-checkout
```

## Build

```bash
cd installer
make deadops   # builds ../deadops-ps5/payload and copies deadops-ps5.elf into assets/
make           # builds dist/PPSA99002/
```

`tools/build.sh` runs the boilerplate's pipeline:

1. Compile `src/main.c` plus the app startup (`tooling/native/app_crt.cpp`, `app_cpp_runtime.cpp`)
2. Link with `tooling/native/ps5-pie.ld`
3. Convert the ELF into an SCE executable (`ps5-native-tool link`)
4. fSELF-sign it into `eboot.bin`

Skipping step 3 produces a payload ELF, which the home screen refuses with CE-100008-9.

Before changing `assets/_clientids.gsc`, check it with
`python3 ../deadops-ps5/host/check_gsc.py assets/_clientids.gsc`: an unknown function name crashes the game.
The check needs the decrypted game executable at `deadops-ps5/re/codzm.plain.elf`, which is not in this repository.

## Output

```
dist/PPSA99002/
  eboot.bin
  sce_sys/param.json, icon0.png
  sce_module/libc.prx
  assets/_clientids.gsc, deadops-ps5.elf
```

## On the PS5

1. Open BO2 Mod Installer -> **Install Mod Menu** (once, and after changing the script)
2. Each session: **Load Mods (send DeadOps)**, then start Zombies within 5 minutes
3. In a Zombies match, double-tap melee (R3) to open the menu
