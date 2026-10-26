/*
 * Standalone entry point — use this if the loader calls main() instead of init()
 * Wraps the same mod menu with a main() entry point.
 */

main()
{
    replaceFunc(maps/mp/zombies/_zm::main_start, ::custom_main_start);
}

custom_main_start()
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

    self waittill("spawned_player");

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

    self iPrintLnBold("^5Mod Menu Loaded — Double-tap [{+melee}]");
}

// ========================================================
//  HUD
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

createBar(align, relative, x, y, width, height, color, sort, alpha)
{
    bar = newClientHudElem(self);
    bar.elemType = "bar";
    bar.width = width;
    bar.height = height;
    bar.align = align;
    bar.relative = relative;
    bar.color = color;
    bar.sort = sort;
    bar.alpha = alpha;
    bar setPoint(align, relative, x, y);
    bar setShader("white", width, height);
    return bar;
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
            items[0] = "God Mode: " + onOff(self.godMode);
            items[1] = "No Clip: " + onOff(self.noClip);
            items[2] = "Third Person: " + onOff(self.thirdPerson);
            items[3] = "Speed x1";
            items[4] = "Speed x2";
            items[5] = "Speed x3";
            items[6] = "Revive Self";
            items[7] = "Suicide";
            items[8] = "<< Back";
            break;
        case "weapons":
            items[0] = "Infinite Ammo: " + onOff(self.infiniteAmmo);
            items[1] = "Max Ammo";
            items[2] = "Ray Gun";
            items[3] = "Ray Gun MK2";
            items[4] = "War Machine";
            items[5] = "HAMR";
            items[6] = "Galil";
            items[7] = "RPD";
            items[8] = "All Perks";
            items[9] = "Drop Weapons";
            items[10] = "<< Back";
            break;
        case "zombies":
            items[0] = "50,000 Points";
            items[1] = "500,000 Points";
            items[2] = "Max Points";
            items[3] = "Kill All Zombies";
            items[4] = "Skip 5 Rounds";
            items[5] = "Skip 10 Rounds";
            items[6] = "Freeze Zombies: " + onOff(self.zombiesFrozen);
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

onOff(v)
{
    if (v) return "^2ON";
    return "^1OFF";
}

// ========================================================
//  MENU CONTROLS
// ========================================================

menuWatcher()
{
    self endon("disconnect");
    self endon("death");
    level endon("game_ended");

    self notifyOnPlayerCommand("btn_melee", "+melee");
    self notifyOnPlayerCommand("btn_up", "+actionslot 1");
    self notifyOnPlayerCommand("btn_down", "+actionslot 2");
    self notifyOnPlayerCommand("btn_left", "+actionslot 3");
    self notifyOnPlayerCommand("btn_right", "+actionslot 4");

    self.lastMelee = 0;

    for (;;)
    {
        ev = self waittill_any_return("btn_melee", "btn_up", "btn_down", "btn_left", "btn_right");

        if (ev == "btn_melee")
        {
            t = getTime();
            if (t - self.lastMelee < 400)
            {
                if (self.menuOpen)
                    self closeMenu();
                else
                    self openMenu();
                self.lastMelee = 0;
            }
            else
                self.lastMelee = t;
            continue;
        }

        if (!self.menuOpen) continue;

        items = getMenuItems(self.subMenu);
        mx = items.size - 1;

        if (ev == "btn_up")
        {
            self.menuIndex--;
            if (self.menuIndex < 0) self.menuIndex = mx;
            self drawMenu();
        }
        else if (ev == "btn_down")
        {
            self.menuIndex++;
            if (self.menuIndex > mx) self.menuIndex = 0;
            self drawMenu();
        }
        else if (ev == "btn_right")
        {
            self execOption();
        }
        else if (ev == "btn_left")
        {
            if (self.subMenu != "main")
            {
                self.subMenu = "main";
                self.menuIndex = 0;
                self drawMenu();
            }
            else
                self closeMenu();
        }
    }
}

// ========================================================
//  MENU RENDERING
// ========================================================

openMenu()
{
    if (self.menuOpen) return;
    self.menuOpen = true;
    self.subMenu = "main";
    self.menuIndex = 0;
    self freezeControls(false);

    self.m_bg = self createBar("LEFT", "TOP", 20, 60, 260, 300, (0, 0, 0), 0, 0.7);
    self.m_hdr = self createBar("LEFT", "TOP", 20, 60, 260, 30, (0.05, 0.4, 0.7), 1, 0.9);
    self.m_sel = self createBar("LEFT", "TOP", 20, 94, 260, 22, (0.1, 0.6, 1.0), 1, 0.35);
    self.m_title = self createText("hudbig", 0.55, "LEFT", "TOP", 28, 62, 2, 1, "MOD MENU");
    self.m_rows = [];
    for (i = 0; i < 12; i++)
        self.m_rows[i] = self createText("hudsmall", 0.85, "LEFT", "TOP", 30, 96 + (i * 24), 2, 1, "");

    self drawMenu();
}

closeMenu()
{
    if (!self.menuOpen) return;
    self.menuOpen = false;

    self.m_bg destroy();
    self.m_hdr destroy();
    self.m_sel destroy();
    self.m_title destroy();
    for (i = 0; i < self.m_rows.size; i++)
        self.m_rows[i] destroy();
}

drawMenu()
{
    if (!self.menuOpen) return;
    items = getMenuItems(self.subMenu);

    titles = [];
    titles["main"] = "MOD MENU";
    titles["self"] = "SELF";
    titles["weapons"] = "WEAPONS";
    titles["zombies"] = "ZOMBIES";
    titles["teleport"] = "TELEPORT";
    titles["game"] = "GAME";
    self.m_title setText(titles[self.subMenu]);

    for (i = 0; i < 12; i++)
    {
        if (i < items.size)
        {
            p = (i == self.menuIndex) ? "^5> " : "  ";
            self.m_rows[i] setText(p + items[i]);
            self.m_rows[i].alpha = 1;
        }
        else
        {
            self.m_rows[i] setText("");
            self.m_rows[i].alpha = 0;
        }
    }
    self.m_sel setPoint("LEFT", "TOP", 20, 94 + (self.menuIndex * 24));
    h = 44 + (items.size * 24) + 10;
    self.m_bg setShader("white", 260, int(h));
}

// ========================================================
//  OPTION DISPATCHER
// ========================================================

execOption()
{
    items = getMenuItems(self.subMenu);
    sel = items[self.menuIndex];

    if (sel == "<< Back")
    {
        self.subMenu = "main";
        self.menuIndex = 0;
        self drawMenu();
        return;
    }

    if (self.subMenu == "main")
    {
        subs = [];
        subs[0] = "self";
        subs[1] = "weapons";
        subs[2] = "zombies";
        subs[3] = "teleport";
        subs[4] = "game";
        self.subMenu = subs[self.menuIndex];
        self.menuIndex = 0;
        self drawMenu();
        return;
    }

    idx = self.menuIndex;

    if (self.subMenu == "self")
    {
        if (idx == 0) self doGodMode();
        else if (idx == 1) self doNoClip();
        else if (idx == 2) self doThirdPerson();
        else if (idx == 3) { self setMoveSpeedScale(1.0); self iPrintLn("Speed: ^5x1"); }
        else if (idx == 4) { self setMoveSpeedScale(2.0); self iPrintLn("Speed: ^5x2"); }
        else if (idx == 5) { self setMoveSpeedScale(3.0); self iPrintLn("Speed: ^5x3"); }
        else if (idx == 6) { self iPrintLn("^2Revived"); self.health = self.maxhealth; }
        else if (idx == 7) self suicide();
    }
    else if (self.subMenu == "weapons")
    {
        if (idx == 0) { self.infiniteAmmo = !self.infiniteAmmo; self iPrintLn("Infinite Ammo: " + onOff(self.infiniteAmmo)); }
        else if (idx == 1) self doMaxAmmo();
        else if (idx == 2) self doGiveWeapon("ray_gun_zm");
        else if (idx == 3) self doGiveWeapon("raygun_mark2_zm");
        else if (idx == 4) self doGiveWeapon("usrpg_zm");
        else if (idx == 5) self doGiveWeapon("hamr_zm");
        else if (idx == 6) self doGiveWeapon("galil_zm");
        else if (idx == 7) self doGiveWeapon("rpd_zm");
        else if (idx == 8) self doAllPerks();
        else if (idx == 9) { self takeAllWeapons(); self giveWeapon("knife_zm"); self iPrintLn("^1Dropped all"); }
    }
    else if (self.subMenu == "zombies")
    {
        if (idx == 0) { self.score = 50000; self iPrintLn("^2+50k"); }
        else if (idx == 1) { self.score = 500000; self iPrintLn("^2+500k"); }
        else if (idx == 2) { self.score = 4294967; self iPrintLn("^2MAX"); }
        else if (idx == 3) self doKillAll();
        else if (idx == 4) { level.round_number += 5; self iPrintLn("^2Round " + level.round_number); }
        else if (idx == 5) { level.round_number += 10; self iPrintLn("^2Round " + level.round_number); }
        else if (idx == 6) self doFreezeZombies();
    }
    else if (self.subMenu == "teleport")
    {
        if (idx == 0) { self.savedOrigin = self.origin; self.savedAngles = self getPlayerAngles(); self iPrintLn("^2Saved"); }
        else if (idx == 1)
        {
            if (isDefined(self.savedOrigin))
            {
                self setOrigin(self.savedOrigin);
                self setPlayerAngles(self.savedAngles);
                self iPrintLn("^2Loaded");
            }
            else self iPrintLn("^1No save");
        }
        else if (idx == 2) { a = self getPlayerAngles(); f = anglesToForward(a); self setOrigin(self.origin + (f * 200)); }
        else if (idx == 3) self setOrigin(self.origin + (0, 0, 500));
    }
    else if (self.subMenu == "game")
    {
        if (idx == 0) { level.zombie_power_on = true; self iPrintLn("^2Power ON"); }
        else if (idx == 1) self doOpenDoors();
        else if (idx == 2) map_restart(false);
        else if (idx == 3) level notify("end_game");
    }
    self drawMenu();
}

// ========================================================
//  MOD FUNCTIONS
// ========================================================

doGodMode()
{
    self.godMode = !self.godMode;
    if (self.godMode)
    {
        self enableInvulnerability();
        self.maxhealth = 999999;
        self.health = self.maxhealth;
    }
    else
    {
        self disableInvulnerability();
        self.maxhealth = 100;
        self.health = 100;
    }
    self iPrintLn("God Mode: " + onOff(self.godMode));
}

doNoClip()
{
    self.noClip = !self.noClip;
    if (self.noClip)
        self thread ncLoop();
    else
        self notify("stop_nc");
    self iPrintLn("No Clip: " + onOff(self.noClip));
}

ncLoop()
{
    self endon("disconnect");
    self endon("death");
    self endon("stop_nc");
    for (;;)
    {
        if (self jumpButtonPressed())
        {
            a = self getPlayerAngles();
            f = anglesToForward(a);
            self setOrigin(self.origin + (f * 25));
        }
        wait 0.05;
    }
}

doThirdPerson()
{
    self.thirdPerson = !self.thirdPerson;
    self setClientDvar("cg_thirdPerson", self.thirdPerson);
    if (self.thirdPerson)
        self setClientDvar("cg_thirdPersonRange", 120);
    self iPrintLn("Third Person: " + onOff(self.thirdPerson));
}

doMaxAmmo()
{
    w = self getWeaponsList();
    for (i = 0; i < w.size; i++)
        self giveMaxAmmo(w[i]);
    self iPrintLn("^2Max Ammo");
}

doGiveWeapon(name)
{
    w = self getWeaponsList();
    if (w.size >= 2)
        self takeWeapon(self getCurrentWeapon());
    self giveWeapon(name);
    self switchToWeapon(name);
    self giveMaxAmmo(name);
    self iPrintLn("^2Gave " + name);
}

doAllPerks()
{
    p = [];
    p[p.size] = "specialty_quickrevive";
    p[p.size] = "specialty_armorvest";
    p[p.size] = "specialty_rof";
    p[p.size] = "specialty_fastreload";
    p[p.size] = "specialty_longersprint";
    p[p.size] = "specialty_deadshot";
    p[p.size] = "specialty_additionalprimaryweapon";
    p[p.size] = "specialty_nomotionsensor";
    for (i = 0; i < p.size; i++)
        if (!self hasPerk(p[i]))
            self setPerk(p[i]);
    self iPrintLn("^2All Perks!");
}

doKillAll()
{
    z = getAiArray(level.zombie_team);
    c = 0;
    for (i = 0; i < z.size; i++)
    {
        if (isDefined(z[i]) && isAlive(z[i]))
        {
            z[i] doDamage(z[i].health + 1000, z[i].origin);
            c++;
        }
    }
    self iPrintLn("^2Killed " + c);
}

doFreezeZombies()
{
    self.zombiesFrozen = !self.zombiesFrozen;
    z = getAiArray(level.zombie_team);
    for (i = 0; i < z.size; i++)
        if (isDefined(z[i]))
            z[i] freezeControls(self.zombiesFrozen);
    self iPrintLn("Freeze: " + onOff(self.zombiesFrozen));
}

doOpenDoors()
{
    d = getEntArray("zombie_door", "targetname");
    for (i = 0; i < d.size; i++)
        if (isDefined(d[i]))
            d[i] notify("trigger", self);
    d = getEntArray("zombie_debris", "targetname");
    for (i = 0; i < d.size; i++)
        if (isDefined(d[i]))
            d[i] notify("trigger", self);
    self iPrintLn("^2Doors Opened");
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
            w = self getCurrentWeapon();
            if (isDefined(w) && w != "none")
            {
                self setWeaponAmmoClip(w, weaponClipSize(w));
                self setWeaponAmmoStock(w, 999);
            }
        }
    }
}
