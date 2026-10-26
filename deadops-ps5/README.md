# DeadOps

Load your own GSC scripts in **Black Ops** and **Black Ops II** on PS4/PS5.

You don't need to compile anything. Just drop your plain `.gsc` file in the right folder.

---

## Setup

1. Copy `DeadOps.sprx` to `/data/DeadOps/` on your console.
2. Load `DeadOps.sprx` into the game however you wish. (NexusFramework)
3. Place your script in the folder for the current game. (See the table below)
4. Start a match. (**You can just edit and reload the match with changes at any time**)

## Where To Put Your Script?

| Game | Folder | File Name |
|---|---|---|
| Black Ops II Campaign | `/data/DeadOps/t6_sp/` | `_callbacksetup.gsc` |
| Black Ops II Zombies | `/data/DeadOps/t6_zm/` | `_clientids.gsc` |
| Black Ops II Multiplayer | `/data/DeadOps/t6_mp/` | `_clientids.gsc` |
| Black Ops Multiplayer | `/data/DeadOps/t5_mp/` | `_clientids.gsc` |
| Black Ops Campaign / Zombies | `/data/DeadOps/t5_cp/` | `cmp.gsc` |

**Example** (Black Ops II Zombies):

```
/data/DeadOps/t6_zm/_clientids.gsc
```

## Replacing Other Scripts (Optional)

You can replace any other game script too. Use its full game path in **lowercase**, inside your game's folder:

```
/data/DeadOps/t6_zm/maps/mp/zombies/_zm_utility.gsc
```

## Credits

- **ItsJoKerZz** — DeadOps
- **xensik** — gsc-tool
- **flatz** — ELF downgrade script

## License
[MIT](LICENSE)