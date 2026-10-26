# BO2 Zombies Mod Menu — PS5 Setup Guide

## What's Included

| File | Description |
|------|-------------|
| `zm_mod_menu.gsc` | Primary mod menu (uses `init()` entry — for DeadOps loader) |
| `zm_mod_menu_standalone.gsc` | Alternate version (uses `main()` + `replaceFunc`) |

## Controls

| Input | Action |
|-------|--------|
| **Double-tap Melee (R3 stick click)** | Open / Close menu |
| **D-Pad Up / Down** | Navigate options |
| **D-Pad Right** | Select option |
| **D-Pad Left** | Back / Close |

## Features

### Self
- God Mode (invulnerability toggle)
- No Clip (fly through walls — hold Jump to move forward)
- Third Person camera toggle
- Speed multiplier (x1, x2, x3)
- Revive Self
- Suicide

### Weapons
- Infinite Ammo toggle (auto-refills clip + stock)
- Max Ammo (all held weapons)
- Give specific weapons: Ray Gun, Ray Gun MK2, War Machine, HAMR, Galil, RPD
- Give All Perks (Quick Revive, Juggernog, Double Tap, Speed Cola, Stamin-Up, Deadshot, Mule Kick, Tombstone/Who's Who)
- Drop All Weapons

### Zombies
- Set Points: 50k / 500k / Max
- Kill All Zombies
- Skip 5 or 10 Rounds
- Freeze/Unfreeze Zombies

### Teleport
- Save Position (stores your location and angle)
- Load Position (teleport back to saved spot)
- Teleport Forward (200 units in look direction)
- Teleport Up (500 units)

### Game Settings
- Toggle Power (enables power without hitting the switch)
- Open All Doors (opens zombie barriers and debris)
- Restart Map
- End Game

---

## Installation — DeadOps Loader (Recommended)

DeadOps is a GSC loader for PS4/PS5 BO2 that compiles scripts at load time.

### Prerequisites
- Jailbroken PS5 with NexusFramework (or equivalent payload loader)
- DeadOps SPRX installed
- FTP access to your PS5

### Steps

1. **FTP into your PS5** and navigate to:
   ```
   /data/DeadOps/t6_zm/
   ```
   Create the directory if it doesn't exist.

2. **Copy the GSC file:**
   - Upload `zm_mod_menu.gsc` to `/data/DeadOps/t6_zm/zm_mod_menu.gsc`

3. **Launch Black Ops II** and start a Zombies match (any map).

4. **Double-tap melee** to open the mod menu.

---

## Installation — Manual .ff Injection (Advanced)

If not using DeadOps, you can inject the compiled GSC into a fast file.

### Steps

1. **Compile** the GSC to T6 bytecode using [gsc-tool](https://github.com/xensik/gsc-tool):
   ```
   gsc-tool comp t6 zm_mod_menu.gsc
   ```

2. **Extract** `patch_zm.ff` using ZoneTool or a T6 fast file extractor.

3. **Inject** the compiled `.gscc` into the zone, adding a reference in the script asset list.

4. **Rebuild** the .ff file and replace `patch_zm.ff` in:
   ```
   PPSA34502-app0/zone/all/patch_zm.ff
   ```

5. **Repackage** the game files for your PS5.

---

## Weapon Names Reference

These are internal weapon names for T6 Zombies. Use these if you want to add more weapons to the menu:

| Display Name | Internal Name |
|-------------|---------------|
| Ray Gun | `ray_gun_zm` |
| Ray Gun MK2 | `raygun_mark2_zm` |
| War Machine | `usrpg_zm` |
| HAMR | `hamr_zm` |
| Galil | `galil_zm` |
| RPD | `rpd_zm` |
| AN-94 | `an94_zm` |
| PDW-57 | `pdw57_zm` |
| MTAR | `tar21_zm` |
| DSR-50 | `dsr50_zm` |
| Executioner | `executioner_zm` |
| Blundergat | `blundergat_zm` |
| Paralyzer | `slowgun_zm` |
| LSAT | `lsat_zm` |
| M8A1 | `xm8_zm` |
| Remington (shotgun) | `870mcs_zm` |
| SVU-AS | `svu_zm` |
| KSG | `ksg_zm` |

## Perk Specialties Reference

| Perk | Internal Name |
|------|---------------|
| Quick Revive | `specialty_quickrevive` |
| Juggernog | `specialty_armorvest` |
| Double Tap 2.0 | `specialty_rof` |
| Speed Cola | `specialty_fastreload` |
| Stamin-Up | `specialty_longersprint` |
| Deadshot Daiquiri | `specialty_deadshot` |
| Mule Kick | `specialty_additionalprimaryweapon` |
| Who's Who / Tombstone | `specialty_nomotionsensor` |
| Electric Cherry | `specialty_grenadepulldeath` |
| Vulture Aid | `specialty_otherapparition` |

## Notes

- This is for **local/offline play only**
- Works on all Zombies maps (TranZit, Town, Farm, Bus Depot, Die Rise, Mob of the Dead, Buried, Origins, Nuketown)
- Some weapons are map-specific; giving a weapon not available on the current map may cause issues
- The "Open All Doors" function uses the trigger system, so it simulates buying each barrier
- Round skipping changes `level.round_number` but doesn't spawn the corresponding zombie wave — use Kill All to clear the current wave after skipping
