/*
 * Black Ops II Zombies Mod Menu — PS5
 * For use with DeadOps GSC Loader (local/offline only)
 * Place in: /data/DeadOps/t6_zm/
 */

#include maps/mp/zombies/_zm_utility;
#include common_scripts/utility;

init()
{
    level thread onPlayerConnect();
}

onPlayerConnect()
{
    for (;;)
    {
        level waittill("connected", player);
        player thread onPlayerSpawned();
    }
}

onPlayerSpawned()
{
    self endon("disconnect");
    level endon("game_ended");

    self.menuOpen = false;
    self.menuIndex = 0;
    self.subMenu = "main";
    self.godMode = false;
    self.infiniteAmmo = false;
    self.noClip = false;
    self.speedScale = 1.0;
    self.savedOrigin = undefined;
    self.savedAngles = undefined;
    self.zombiesFrozen = false;
    self.thirdPerson = false;

    self thread menuWatcher();
    self thread ammoWatcher();
    self thread welcomeMessage();
}

// ========================================================
//  HUD HELPERS
// ========================================================

createText(font, fontscale, align, relative, x, y, sort, alpha, text)
{
    hud = self createFontString(font, fontscale);
    hud setPoint(align, relative, x, y);
    hud.sort = sort;
    hud.alpha = alpha;
    hud setText(text);
    return hud;
}

createRectangle(align, relative, x, y, width, height, color, sort, alpha)
{
    bar = newClientHudElem(self);
    bar.elemType = "bar";
    bar.width = width;
    bar.height = height;
    bar.align = align;
    bar.relative = relative;
    bar.xOffset = 0;
    bar.children = [];
    bar.color = color;
    bar.sort = sort;
    bar.alpha = alpha;
    bar setPoint(align, relative, x, y);
    bar setShader("white", width, height);
    return bar;
}

// ========================================================
//  WELCOME MESSAGE
// ========================================================

welcomeMessage()
{
    self endon("disconnect");
    wait 3;

    notifyText = self createFontString("hudbig", 1.5);
    notifyText setPoint("CENTER", "CENTER", 0, -50);
    notifyText.alpha = 0;
    notifyText.color = (0.05, 0.7, 1.0);
    notifyText.glowAlpha = 1;
    notifyText.glowColor = (0.05, 0.4, 0.7);
    notifyText setText("Mod Menu Loaded");
    notifyText.alpha = 1;
    wait 0.5;

    instrText = self createFontString("hudsmall", 1.0);
    instrText setPoint("CENTER", "CENTER", 0, -25);
    instrText.alpha = 0;
    instrText.color = (1, 1, 1);
    instrText setText("[{+melee}] x2 to open menu");
    instrText.alpha = 1;

    wait 4;
    notifyText fadeOverTime(1.0);
    notifyText.alpha = 0;
    instrText fadeOverTime(1.0);
    instrText.alpha = 0;
    wait 1.2;
    notifyText destroy();
    instrText destroy();
}

// ========================================================
//  MENU DATA
// ========================================================

getMenuItems(menu)
{
    items = [];
    switch (menu)
    {
        case "main":
            items[0] = "Self";
            items[1] = "Weapons";
            items[2] = "Zombies";
            items[3] = "Teleport";
            items[4] = "Game Settings";
            break;

        case "self":
            items[0] = "God Mode: " + boolStr(self.godMode);
            items[1] = "No Clip: " + boolStr(self.noClip);
            items[2] = "Third Person: " + boolStr(self.thirdPerson);
            items[3] = "Speed x1";
            items[4] = "Speed x2";
            items[5] = "Speed x3";
            items[6] = "Revive Self";
            items[7] = "Suicide";
            items[8] = "<< Back";
            break;

        case "weapons":
            items[0] = "Infinite Ammo: " + boolStr(self.infiniteAmmo);
            items[1] = "Max Ammo (current)";
            items[2] = "Give Ray Gun";
            items[3] = "Give Ray Gun MK2";
            items[4] = "Give War Machine";
            items[5] = "Give HAMR";
            items[6] = "Give Galil";
            items[7] = "Give RPD";
            items[8] = "Give All Perks";
            items[9] = "Take All Weapons";
            items[10] = "<< Back";
            break;

        case "zombies":
            items[0] = "Set Points: 50000";
            items[1] = "Set Points: 500000";
            items[2] = "Set Points: Max";
            items[3] = "Kill All Zombies";
            items[4] = "Skip 5 Rounds";
            items[5] = "Skip 10 Rounds";
            items[6] = "Freeze Zombies: " + boolStr(self.zombiesFrozen);
            items[7] = "<< Back";
            break;

        case "teleport":
            items[0] = "Save Position";
            items[1] = "Load Position";
            items[2] = "Teleport Forward";
            items[3] = "Teleport Up";
            items[4] = "<< Back";
            break;

        case "game":
            items[0] = "Toggle Power";
            items[1] = "Open All Doors";
            items[2] = "Restart Map";
            items[3] = "End Game";
            items[4] = "<< Back";
            break;
    }
    return items;
}

boolStr(val)
{
    if (val)
        return "^2ON";
    return "^1OFF";
}

// ========================================================
//  MENU INPUT WATCHER
// ========================================================

menuWatcher()
{
    self endon("disconnect");
    self endon("death");
    level endon("game_ended");

    self notifyOnPlayerCommand("melee", "+melee");
    self notifyOnPlayerCommand("ads", "+speed_throw");
    self notifyOnPlayerCommand("dpad_up", "+actionslot 1");
    self notifyOnPlayerCommand("dpad_down", "+actionslot 2");
    self notifyOnPlayerCommand("dpad_left", "+actionslot 3");
    self notifyOnPlayerCommand("dpad_right", "+actionslot 4");

    self.lastMelee = 0;

    for (;;)
    {
        event = self waittill_any_return("melee", "dpad_up", "dpad_down", "dpad_left", "dpad_right");

        if (event == "melee")
        {
            now = getTime();
            if (now - self.lastMelee < 400)
            {
                if (self.menuOpen)
                    self closeMenu();
                else
                    self openMenu();
                self.lastMelee = 0;
            }
            else
            {
                self.lastMelee = now;
            }
            continue;
        }

        if (!self.menuOpen)
            continue;

        items = getMenuItems(self.subMenu);
        maxIdx = items.size - 1;

        switch (event)
        {
            case "dpad_up":
                self.menuIndex--;
                if (self.menuIndex < 0)
                    self.menuIndex = maxIdx;
                self refreshMenu();
                break;

            case "dpad_down":
                self.menuIndex++;
                if (self.menuIndex > maxIdx)
                    self.menuIndex = 0;
                self refreshMenu();
                break;

            case "dpad_right":
                self selectOption();
                break;

            case "dpad_left":
                if (self.subMenu != "main")
                {
                    self.subMenu = "main";
                    self.menuIndex = 0;
                    self refreshMenu();
                }
                else
                {
                    self closeMenu();
                }
                break;
        }
    }
}

// ========================================================
//  MENU OPEN / CLOSE / REFRESH
// ========================================================

openMenu()
{
    if (self.menuOpen)
        return;

    self.menuOpen = true;
    self.subMenu = "main";
    self.menuIndex = 0;
    self freezeControls(false);

    self.hud = [];

    self.hud["bg"] = self createRectangle("LEFT", "TOP", 20, 60, 260, 340, (0, 0, 0), 0, 0.65);
    self.hud["header_bg"] = self createRectangle("LEFT", "TOP", 20, 60, 260, 32, (0.05, 0.4, 0.7), 1, 0.85);
    self.hud["title"] = self createText("hudbig", 0.6, "LEFT", "TOP", 30, 63, 2, 1, "MOD MENU");
    self.hud["title"].color = (1, 1, 1);
    self.hud["scrollbar"] = self createRectangle("LEFT", "TOP", 20, 96, 260, 22, (0.05, 0.7, 1.0), 1, 0.4);
    self.hud["instr"] = self createText("hudsmall", 0.65, "LEFT", "TOP", 25, 380, 2, 0.7, "D-Pad: Navigate | Right: Select | Left: Back");
    self.hud["instr"].color = (0.7, 0.7, 0.7);

    self.hud["items"] = [];
    for (i = 0; i < 12; i++)
    {
        self.hud["items"][i] = self createText("hudsmall", 0.85, "LEFT", "TOP", 30, 98 + (i * 24), 2, 1, "");
    }

    self refreshMenu();
}

closeMenu()
{
    if (!self.menuOpen)
        return;

    self.menuOpen = false;

    if (isDefined(self.hud))
    {
        if (isDefined(self.hud["bg"])) self.hud["bg"] destroy();
        if (isDefined(self.hud["header_bg"])) self.hud["header_bg"] destroy();
        if (isDefined(self.hud["title"])) self.hud["title"] destroy();
        if (isDefined(self.hud["scrollbar"])) self.hud["scrollbar"] destroy();
        if (isDefined(self.hud["instr"])) self.hud["instr"] destroy();
        if (isDefined(self.hud["items"]))
        {
            for (i = 0; i < self.hud["items"].size; i++)
            {
                if (isDefined(self.hud["items"][i]))
                    self.hud["items"][i] destroy();
            }
        }
    }
    self.hud = undefined;
}

refreshMenu()
{
    if (!self.menuOpen)
        return;

    items = getMenuItems(self.subMenu);

    titleMap = [];
    titleMap["main"] = "MOD MENU";
    titleMap["self"] = "SELF";
    titleMap["weapons"] = "WEAPONS";
    titleMap["zombies"] = "ZOMBIES";
    titleMap["teleport"] = "TELEPORT";
    titleMap["game"] = "GAME SETTINGS";

    if (isDefined(titleMap[self.subMenu]))
        self.hud["title"] setText(titleMap[self.subMenu]);

    for (i = 0; i < 12; i++)
    {
        if (i < items.size)
        {
            prefix = "  ";
            if (i == self.menuIndex)
                prefix = "^5> ";
            self.hud["items"][i] setText(prefix + items[i]);
            self.hud["items"][i].alpha = 1;
        }
        else
        {
            self.hud["items"][i] setText("");
            self.hud["items"][i].alpha = 0;
        }
    }

    yPos = 96 + (self.menuIndex * 24);
    self.hud["scrollbar"] setPoint("LEFT", "TOP", 20, yPos);

    bgHeight = 45 + (items.size * 24) + 20;
    if (bgHeight < 100) bgHeight = 100;
    self.hud["bg"] setShader("white", 260, int(bgHeight));
    self.hud["instr"] setPoint("LEFT", "TOP", 25, 65 + int(bgHeight));
}

// ========================================================
//  OPTION EXECUTION
// ========================================================

selectOption()
{
    items = getMenuItems(self.subMenu);
    selected = items[self.menuIndex];

    if (selected == "<< Back")
    {
        self.subMenu = "main";
        self.menuIndex = 0;
        self refreshMenu();
        return;
    }

    switch (self.subMenu)
    {
        case "main":
            self selectMain();
            break;
        case "self":
            self selectSelf();
            break;
        case "weapons":
            self selectWeapons();
            break;
        case "zombies":
            self selectZombies();
            break;
        case "teleport":
            self selectTeleport();
            break;
        case "game":
            self selectGame();
            break;
    }
}

selectMain()
{
    subMenus = [];
    subMenus[0] = "self";
    subMenus[1] = "weapons";
    subMenus[2] = "zombies";
    subMenus[3] = "teleport";
    subMenus[4] = "game";

    if (isDefined(subMenus[self.menuIndex]))
    {
        self.subMenu = subMenus[self.menuIndex];
        self.menuIndex = 0;
        self refreshMenu();
    }
}

// ========================================================
//  SELF OPTIONS
// ========================================================

selectSelf()
{
    switch (self.menuIndex)
    {
        case 0: // God Mode
            self toggleGodMode();
            break;
        case 1: // No Clip
            self toggleNoClip();
            break;
        case 2: // Third Person
            self toggleThirdPerson();
            break;
        case 3: // Speed x1
            self setSpeed(1.0);
            break;
        case 4: // Speed x2
            self setSpeed(2.0);
            break;
        case 5: // Speed x3
            self setSpeed(3.0);
            break;
        case 6: // Revive Self
            self reviveSelf();
            break;
        case 7: // Suicide
            self suicide();
            break;
    }
    self refreshMenu();
}

toggleGodMode()
{
    self.godMode = !self.godMode;
    if (self.godMode)
    {
        self enableInvulnerability();
        self.maxhealth = 999999;
        self.health = self.maxhealth;
        self iPrintLn("^2God Mode: ON");
    }
    else
    {
        self disableInvulnerability();
        self.maxhealth = 100;
        self.health = self.maxhealth;
        self iPrintLn("^1God Mode: OFF");
    }
}

toggleNoClip()
{
    self.noClip = !self.noClip;
    if (self.noClip)
    {
        self thread noClipLoop();
        self iPrintLn("^2No Clip: ON — Hold [{+gostand}] to fly");
    }
    else
    {
        self notify("stop_noclip");
        self iPrintLn("^1No Clip: OFF");
    }
}

noClipLoop()
{
    self endon("disconnect");
    self endon("death");
    self endon("stop_noclip");

    for (;;)
    {
        if (self jumpButtonPressed())
        {
            angles = self getPlayerAngles();
            forward = anglesToForward(angles);
            origin = self.origin + (forward * 30);
            self setOrigin(origin);
        }
        wait 0.05;
    }
}

toggleThirdPerson()
{
    self.thirdPerson = !self.thirdPerson;
    if (self.thirdPerson)
    {
        self setClientDvar("cg_thirdPerson", 1);
        self setClientDvar("cg_thirdPersonRange", 120);
        self iPrintLn("^2Third Person: ON");
    }
    else
    {
        self setClientDvar("cg_thirdPerson", 0);
        self iPrintLn("^1Third Person: OFF");
    }
}

setSpeed(scale)
{
    self.speedScale = scale;
    self setMoveSpeedScale(scale);
    self iPrintLn("Speed: ^5" + scale + "x");
}

reviveSelf()
{
    if (self.sessionstate == "spectator" || (isDefined(self.down) && self.down))
    {
        self.health = self.maxhealth;
        self revivePlayer();
        self iPrintLn("^2Revived!");
    }
    else
    {
        self iPrintLn("^3Not downed.");
    }
}

// ========================================================
//  WEAPON OPTIONS
// ========================================================

selectWeapons()
{
    switch (self.menuIndex)
    {
        case 0: // Infinite Ammo toggle
            self.infiniteAmmo = !self.infiniteAmmo;
            if (self.infiniteAmmo)
                self iPrintLn("^2Infinite Ammo: ON");
            else
                self iPrintLn("^1Infinite Ammo: OFF");
            break;
        case 1: // Max Ammo current
            self giveMaxAmmoAll();
            break;
        case 2: // Ray Gun
            self giveWeaponSafe("ray_gun_zm");
            break;
        case 3: // Ray Gun MK2
            self giveWeaponSafe("raygun_mark2_zm");
            break;
        case 4: // War Machine
            self giveWeaponSafe("usrpg_zm");
            break;
        case 5: // HAMR
            self giveWeaponSafe("hamr_zm");
            break;
        case 6: // Galil
            self giveWeaponSafe("galil_zm");
            break;
        case 7: // RPD
            self giveWeaponSafe("rpd_zm");
            break;
        case 8: // All Perks
            self giveAllPerks();
            break;
        case 9: // Take All Weapons
            self takeAllWeapons();
            self giveWeapon("knife_zm");
            self giveWeapon("frag_grenade_zm");
            self iPrintLn("^1Weapons cleared");
            break;
    }
    self refreshMenu();
}

giveWeaponSafe(weaponName)
{
    if (!isDefined(weaponName) || weaponName == "")
        return;

    weapons = self getWeaponsList();
    if (weapons.size >= 2)
    {
        current = self getCurrentWeapon();
        self takeWeapon(current);
    }

    self giveWeapon(weaponName);
    self switchToWeapon(weaponName);
    self giveMaxAmmo(weaponName);
    self iPrintLn("^2Gave: " + weaponName);
}

giveMaxAmmoAll()
{
    weapons = self getWeaponsList();
    for (i = 0; i < weapons.size; i++)
    {
        self giveMaxAmmo(weapons[i]);
    }
    self iPrintLn("^2Max ammo for all weapons");
}

giveAllPerks()
{
    perks = [];
    perks[0] = "specialty_quickrevive";
    perks[1] = "specialty_armorvest";
    perks[2] = "specialty_rof";
    perks[3] = "specialty_fastreload";
    perks[4] = "specialty_longersprint";
    perks[5] = "specialty_deadshot";
    perks[6] = "specialty_additionalprimaryweapon";
    perks[7] = "specialty_nomotionsensor";

    for (i = 0; i < perks.size; i++)
    {
        if (!self hasPerk(perks[i]))
            self setPerk(perks[i]);
    }

    self.num_perks = perks.size;
    self iPrintLn("^2All Perks Given!");
}

ammoWatcher()
{
    self endon("disconnect");
    self endon("death");
    level endon("game_ended");

    for (;;)
    {
        wait 0.1;
        if (isDefined(self.infiniteAmmo) && self.infiniteAmmo)
        {
            current = self getCurrentWeapon();
            if (isDefined(current) && current != "none")
            {
                self setWeaponAmmoClip(current, weaponClipSize(current));
                self setWeaponAmmoStock(current, 999);
            }
        }
    }
}

// ========================================================
//  ZOMBIE OPTIONS
// ========================================================

selectZombies()
{
    switch (self.menuIndex)
    {
        case 0: // 50000 points
            self.score = 50000;
            self iPrintLn("^2Points set to 50,000");
            break;
        case 1: // 500000 points
            self.score = 500000;
            self iPrintLn("^2Points set to 500,000");
            break;
        case 2: // Max points
            self.score = 4294967;
            self iPrintLn("^2Points set to MAX");
            break;
        case 3: // Kill all
            self killAllZombies();
            break;
        case 4: // Skip 5 rounds
            level.round_number += 5;
            self iPrintLn("^2Skipped to round " + level.round_number);
            break;
        case 5: // Skip 10 rounds
            level.round_number += 10;
            self iPrintLn("^2Skipped to round " + level.round_number);
            break;
        case 6: // Freeze zombies
            self toggleFreezeZombies();
            break;
    }
    self refreshMenu();
}

killAllZombies()
{
    zombies = getAiArray(level.zombie_team);
    count = 0;
    for (i = 0; i < zombies.size; i++)
    {
        if (isDefined(zombies[i]) && isAlive(zombies[i]))
        {
            zombies[i] doDamage(zombies[i].health + 1000, zombies[i].origin);
            count++;
        }
    }
    self iPrintLn("^2Killed " + count + " zombies");
}

toggleFreezeZombies()
{
    self.zombiesFrozen = !self.zombiesFrozen;
    zombies = getAiArray(level.zombie_team);
    for (i = 0; i < zombies.size; i++)
    {
        if (isDefined(zombies[i]))
        {
            if (self.zombiesFrozen)
            {
                zombies[i] freezeControls(true);
            }
            else
            {
                zombies[i] freezeControls(false);
            }
        }
    }
    if (self.zombiesFrozen)
        self iPrintLn("^2Zombies Frozen");
    else
        self iPrintLn("^1Zombies Unfrozen");
}

// ========================================================
//  TELEPORT OPTIONS
// ========================================================

selectTeleport()
{
    switch (self.menuIndex)
    {
        case 0: // Save Position
            self.savedOrigin = self.origin;
            self.savedAngles = self getPlayerAngles();
            self iPrintLn("^2Position Saved");
            break;
        case 1: // Load Position
            if (isDefined(self.savedOrigin))
            {
                self setOrigin(self.savedOrigin);
                self setPlayerAngles(self.savedAngles);
                self iPrintLn("^2Teleported to saved position");
            }
            else
            {
                self iPrintLn("^1No saved position");
            }
            break;
        case 2: // Forward
            angles = self getPlayerAngles();
            forward = anglesToForward(angles);
            self setOrigin(self.origin + (forward * 200));
            self iPrintLn("^2Teleported forward");
            break;
        case 3: // Up
            self setOrigin(self.origin + (0, 0, 500));
            self iPrintLn("^2Teleported up");
            break;
    }
}

// ========================================================
//  GAME OPTIONS
// ========================================================

selectGame()
{
    switch (self.menuIndex)
    {
        case 0: // Toggle Power
            if (isDefined(level.zombie_power_on))
            {
                level.zombie_power_on = !level.zombie_power_on;
                if (level.zombie_power_on)
                    self iPrintLn("^2Power: ON");
                else
                    self iPrintLn("^1Power: OFF");
            }
            else
            {
                level.zombie_power_on = true;
                self iPrintLn("^2Power: ON");
            }
            break;
        case 1: // Open All Doors
            self openAllDoors();
            break;
        case 2: // Restart Map
            map_restart(false);
            break;
        case 3: // End Game
            level notify("end_game");
            self iPrintLn("^1Game ended");
            break;
    }
}

openAllDoors()
{
    doors = getEntArray("zombie_door", "targetname");
    for (i = 0; i < doors.size; i++)
    {
        if (isDefined(doors[i]))
        {
            doors[i] notify("trigger", self);
        }
    }

    debris = getEntArray("zombie_debris", "targetname");
    for (i = 0; i < debris.size; i++)
    {
        if (isDefined(debris[i]))
        {
            debris[i] notify("trigger", self);
        }
    }

    self iPrintLn("^2All doors opened");
}
