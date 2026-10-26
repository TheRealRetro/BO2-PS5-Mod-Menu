// Black Ops II Zombies Mod Menu - PS5 / DeadOps-PS5
// Replaces _clientids.gsc - preserves original client ID logic
// Controls: double-tap melee to open, d-pad to navigate
//
// Uses engine builtins only (no #include): every function and HUD field here
// is checked against the PS5 executable, since an unknown name at script
// link time takes the game down.

init()
{
    // --- original _clientids.gsc logic (required) ---
    level.clientid = 0;

    // --- mod menu setup ---
    precacheShader("white");
    level thread onPlayerConnect();
}

onPlayerConnect()
{
    for (;;)
    {
        level waittill("connecting", player);

        // original client ID assignment
        player.clientid = level.clientid;
        level.clientid++;

        // start mod menu for this player
        player thread onPlayerSpawned();
    }
}

onPlayerSpawned()
{
    self endon("disconnect");
    level endon("game_ended");

    // wait for first spawn
    self waittill("spawned_player");

    // state
    self.mn = spawnStruct();
    self.mn.open = false;
    self.mn.idx = 0;
    self.mn.sub = "main";
    self.mn.god = false;
    self.mn.infammo = false;
    self.mn.noclip = false;
    self.mn.frozen = false;
    self.mn.tp = false;
    self.mn.savedPos = undefined;
    self.mn.savedAng = undefined;

    self thread inputLoop();
    self thread ammoLoop();
    self thread showWelcome();

    // handle respawns
    for (;;)
    {
        self waittill("spawned_player");
        if (self.mn.god)
        {
            self enableInvulnerability();
            self.maxhealth = 999999;
            self.health = self.maxhealth;
        }
    }
}

// ============================================================
//  WELCOME
// ============================================================

showWelcome()
{
    self endon("disconnect");
    wait 3;

    msg = self hudText("objective", 2.0, "center", "middle", 0, -50, 3);
    msg.color = (0.1, 0.7, 1.0);
    msg.glowAlpha = 1;
    msg.glowColor = (0.05, 0.35, 0.65);
    msg.alpha = 0;
    msg setText("Mod Menu Loaded");
    msg fadeOverTime(0.4);
    msg.alpha = 1;
    wait 0.5;

    tip = self hudText("default", 1.4, "center", "middle", 0, -20, 3);
    tip.alpha = 0;
    tip setText("Double-tap [{+melee}] to open");
    tip fadeOverTime(0.3);
    tip.alpha = 1;

    wait 4;
    msg fadeOverTime(0.8);
    msg.alpha = 0;
    tip fadeOverTime(0.8);
    tip.alpha = 0;
    wait 1;
    msg destroy();
    tip destroy();
}

// ============================================================
//  HUD HELPERS (builtins only)
// ============================================================

hudText(font, scale, alignX, alignY, x, y, sort)
{
    e = newClientHudElem(self);
    e.font = font;
    e.fontscale = scale;
    e.alignx = alignX;
    e.aligny = alignY;
    e.horzalign = alignX;
    e.vertalign = alignY;
    e.x = x;
    e.y = y;
    e.sort = sort;
    e.foreground = true;
    e.hidewheninmenu = true;
    e.archived = false;
    return e;
}

hudRect(x, y, w, h, color, alpha, sort)
{
    e = newClientHudElem(self);
    e.alignx = "left";
    e.aligny = "top";
    e.horzalign = "user_left";
    e.vertalign = "user_top";
    e.x = x;
    e.y = y;
    e.color = color;
    e.alpha = alpha;
    e.sort = sort;
    e.foreground = true;
    e.hidewheninmenu = true;
    e.archived = false;
    e setShader("white", w, h);
    return e;
}

menuText(font, scale, x, y, sort)
{
    e = self hudText(font, scale, "left", "top", x, y, sort);
    e.horzalign = "user_left";
    e.vertalign = "user_top";
    return e;
}

// ============================================================
//  MENU DATA
// ============================================================

menuItems(menu)
{
    m = [];
    switch (menu)
    {
        case "main":
            m[0] = "Self";
            m[1] = "Weapons";
            m[2] = "Zombies";
            m[3] = "Teleport";
            m[4] = "Game";
            break;
        case "self":
            m[0] = "God Mode: " + tf(self.mn.god);
            m[1] = "No Clip: " + tf(self.mn.noclip);
            m[2] = "Third Person: " + tf(self.mn.tp);
            m[3] = "Speed x1";
            m[4] = "Speed x2";
            m[5] = "Speed x3";
            m[6] = "Revive Self";
            m[7] = "Suicide";
            m[8] = "<< Back";
            break;
        case "weapons":
            m[0] = "Infinite Ammo: " + tf(self.mn.infammo);
            m[1] = "Max Ammo";
            m[2] = "Ray Gun";
            m[3] = "Ray Gun MK2";
            m[4] = "War Machine";
            m[5] = "HAMR";
            m[6] = "Galil";
            m[7] = "RPD";
            m[8] = "AN-94";
            m[9] = "All Perks";
            m[10] = "Drop All Weapons";
            m[11] = "<< Back";
            break;
        case "zombies":
            m[0] = "50,000 Points";
            m[1] = "500,000 Points";
            m[2] = "Max Points";
            m[3] = "Kill All Zombies";
            m[4] = "Skip 5 Rounds";
            m[5] = "Skip 10 Rounds";
            m[6] = "Freeze Zombies: " + tf(self.mn.frozen);
            m[7] = "<< Back";
            break;
        case "teleport":
            m[0] = "Save Position";
            m[1] = "Load Position";
            m[2] = "Teleport Forward";
            m[3] = "Teleport Up";
            m[4] = "<< Back";
            break;
        case "game":
            m[0] = "Turn On Power";
            m[1] = "Open All Doors";
            m[2] = "Restart Map";
            m[3] = "End Game";
            m[4] = "<< Back";
            break;
    }
    return m;
}

menuTitle(menu)
{
    t = [];
    t["main"] = "MOD MENU";
    t["self"] = "SELF";
    t["weapons"] = "WEAPONS";
    t["zombies"] = "ZOMBIES";
    t["teleport"] = "TELEPORT";
    t["game"] = "GAME";
    return t[menu];
}

tf(v)
{
    if (v) return "^2ON";
    return "^1OFF";
}

// ============================================================
//  INPUT LOOP
// ============================================================

pressedButton()
{
    if (self meleeButtonPressed()) return "melee";
    if (self actionSlotOneButtonPressed()) return "up";
    if (self actionSlotTwoButtonPressed()) return "down";
    if (self actionSlotThreeButtonPressed()) return "left";
    if (self actionSlotFourButtonPressed()) return "right";
    return "";
}

// Polls the buttons (BO2 has no notifyOnPlayerCommand). Keeps running across
// downs and respawns; only a disconnect or game end stops it.
inputLoop()
{
    self endon("disconnect");
    level endon("game_ended");

    lastTap = 0;

    for (;;)
    {
        ev = self pressedButton();
        if (ev == "")
        {
            wait 0.05;
            continue;
        }

        if (ev == "melee")
        {
            t = getTime();
            if (t - lastTap < 400)
            {
                if (self.mn.open)
                    self menuClose();
                else
                    self menuOpen();
                lastTap = 0;
            }
            else
                lastTap = t;
        }
        else if (self.mn.open)
        {
            items = menuItems(self.mn.sub);
            last = items.size - 1;

            if (ev == "up")
            {
                self.mn.idx--;
                if (self.mn.idx < 0) self.mn.idx = last;
                self menuDraw();
            }
            else if (ev == "down")
            {
                self.mn.idx++;
                if (self.mn.idx > last) self.mn.idx = 0;
                self menuDraw();
            }
            else if (ev == "right")
            {
                self menuExec();
            }
            else if (ev == "left")
            {
                if (self.mn.sub != "main")
                {
                    self.mn.sub = "main";
                    self.mn.idx = 0;
                    self menuDraw();
                }
                else
                    self menuClose();
            }
        }

        // one action per press: wait for release
        while (self pressedButton() == ev)
            wait 0.05;
    }
}

// ============================================================
//  MENU OPEN / CLOSE / DRAW
// ============================================================

menuOpen()
{
    if (self.mn.open) return;
    self.mn.open = true;
    self.mn.sub = "main";
    self.mn.idx = 0;

    // background, header bar, selection highlight
    self.mn.bg = self hudRect(18, 58, 260, 300, (0, 0, 0), 0.72, 0);
    self.mn.hdr = self hudRect(18, 58, 260, 30, (0.05, 0.35, 0.65), 0.92, 1);
    self.mn.sel = self hudRect(18, 92, 260, 22, (0.1, 0.55, 0.95), 0.45, 1);

    // title text
    self.mn.ttl = self menuText("objective", 1.6, 26, 62, 2);

    // option rows (max 12 visible)
    self.mn.rows = [];
    for (i = 0; i < 12; i++)
        self.mn.rows[i] = self menuText("default", 1.3, 28, 95 + (i * 24), 2);

    // instruction footer
    self.mn.foot = self menuText("default", 1.0, 22, 0, 2);
    self.mn.foot.alpha = 0.6;
    self.mn.foot.color = (0.7, 0.7, 0.7);
    self.mn.foot setText("Up/Down: scroll | Right: select | Left: back");

    self menuDraw();
}

menuClose()
{
    if (!self.mn.open) return;
    self.mn.open = false;

    self.mn.bg destroy();
    self.mn.hdr destroy();
    self.mn.sel destroy();
    self.mn.ttl destroy();
    self.mn.foot destroy();
    for (i = 0; i < self.mn.rows.size; i++)
        self.mn.rows[i] destroy();
}

menuDraw()
{
    if (!self.mn.open) return;

    items = menuItems(self.mn.sub);
    self.mn.ttl setText(menuTitle(self.mn.sub));

    for (i = 0; i < 12; i++)
    {
        if (i < items.size)
        {
            self.mn.rows[i] setText(items[i]);
            self.mn.rows[i].alpha = 1;
        }
        else
        {
            self.mn.rows[i] setText("");
            self.mn.rows[i].alpha = 0;
        }
    }

    // move highlight bar
    self.mn.sel.y = 92 + (self.mn.idx * 24);

    // resize background to fit content
    h = 42 + (items.size * 24) + 12;
    if (h < 100) h = 100;
    self.mn.bg setShader("white", 260, int(h));

    // reposition footer below background
    self.mn.foot.y = 62 + int(h);
}

// ============================================================
//  OPTION DISPATCHER
// ============================================================

menuExec()
{
    items = menuItems(self.mn.sub);
    sel = items[self.mn.idx];

    if (sel == "<< Back")
    {
        self.mn.sub = "main";
        self.mn.idx = 0;
        self menuDraw();
        return;
    }

    // main menu -> open submenu
    if (self.mn.sub == "main")
    {
        subs = [];
        subs[0] = "self";
        subs[1] = "weapons";
        subs[2] = "zombies";
        subs[3] = "teleport";
        subs[4] = "game";
        self.mn.sub = subs[self.mn.idx];
        self.mn.idx = 0;
        self menuDraw();
        return;
    }

    n = self.mn.idx;

    // ---------- SELF ----------
    if (self.mn.sub == "self")
    {
        if (n == 0) self doGod();
        else if (n == 1) self doNoclip();
        else if (n == 2) self doThirdPerson();
        else if (n == 3) self doSpeed(1.0);
        else if (n == 4) self doSpeed(2.0);
        else if (n == 5) self doSpeed(3.0);
        else if (n == 6) self doRevive();
        else if (n == 7) self suicide();
    }
    // ---------- WEAPONS ----------
    else if (self.mn.sub == "weapons")
    {
        if (n == 0) { self.mn.infammo = !self.mn.infammo; self iPrintLn("Infinite Ammo: " + tf(self.mn.infammo)); }
        else if (n == 1) self doMaxAmmo();
        else if (n == 2) self doGive("ray_gun_zm");
        else if (n == 3) self doGive("raygun_mark2_zm");
        else if (n == 4) self doGive("usrpg_zm");
        else if (n == 5) self doGive("hamr_zm");
        else if (n == 6) self doGive("galil_zm");
        else if (n == 7) self doGive("rpd_zm");
        else if (n == 8) self doGive("an94_zm");
        else if (n == 9) self doPerks();
        else if (n == 10)
        {
            self takeAllWeapons();
            self giveWeapon("knife_zm");
            self giveWeapon("frag_grenade_zm");
            self iPrintLn("^1Weapons dropped");
        }
    }
    // ---------- ZOMBIES ----------
    else if (self.mn.sub == "zombies")
    {
        if (n == 0) { self.score = 50000; self iPrintLn("^2Points: 50,000"); }
        else if (n == 1) { self.score = 500000; self iPrintLn("^2Points: 500,000"); }
        else if (n == 2) { self.score = 4294967; self iPrintLn("^2Points: MAX"); }
        else if (n == 3) self doKillAll();
        else if (n == 4) { level.round_number += 5; self iPrintLn("^2Round " + level.round_number); }
        else if (n == 5) { level.round_number += 10; self iPrintLn("^2Round " + level.round_number); }
        else if (n == 6) self doFreeze();
    }
    // ---------- TELEPORT ----------
    else if (self.mn.sub == "teleport")
    {
        if (n == 0)
        {
            self.mn.savedPos = self.origin;
            self.mn.savedAng = self getPlayerAngles();
            self iPrintLn("^2Position saved");
        }
        else if (n == 1)
        {
            if (isDefined(self.mn.savedPos))
            {
                self setOrigin(self.mn.savedPos);
                self setPlayerAngles(self.mn.savedAng);
                self iPrintLn("^2Teleported");
            }
            else
                self iPrintLn("^1No saved position");
        }
        else if (n == 2)
        {
            ang = self getPlayerAngles();
            fwd = anglesToForward(ang);
            self setOrigin(self.origin + (fwd * 200));
            self iPrintLn("^2Forward");
        }
        else if (n == 3)
        {
            self setOrigin(self.origin + (0, 0, 500));
            self iPrintLn("^2Up");
        }
    }
    // ---------- GAME ----------
    else if (self.mn.sub == "game")
    {
        if (n == 0) { level.zombie_power_on = true; level notify("electricity_on"); self iPrintLn("^2Power ON"); }
        else if (n == 1) self doOpenDoors();
        else if (n == 2) map_restart(false);
        else if (n == 3) level notify("end_game");
    }

    self menuDraw();
}

// ============================================================
//  MOD FUNCTIONS
// ============================================================

doGod()
{
    self.mn.god = !self.mn.god;
    if (self.mn.god)
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
    self iPrintLn("God Mode: " + tf(self.mn.god));
}

doNoclip()
{
    self.mn.noclip = !self.mn.noclip;
    if (self.mn.noclip)
        self thread noclipThread();
    else
        self notify("stop_noclip");
    self iPrintLn("No Clip: " + tf(self.mn.noclip));
}

noclipThread()
{
    self endon("disconnect");
    self endon("death");
    self endon("stop_noclip");
    for (;;)
    {
        if (self jumpButtonPressed())
        {
            ang = self getPlayerAngles();
            fwd = anglesToForward(ang);
            self setOrigin(self.origin + (fwd * 25));
        }
        wait 0.05;
    }
}

doThirdPerson()
{
    self.mn.tp = !self.mn.tp;
    // BO2 has no setClientDvar; this is its player method for the camera.
    if (self.mn.tp)
        self setClientThirdPerson(1);
    else
        self setClientThirdPerson(0);
    self iPrintLn("Third Person: " + tf(self.mn.tp));
}

doSpeed(s)
{
    self setMoveSpeedScale(s);
    self iPrintLn("Speed: ^5" + s + "x");
}

doRevive()
{
    self.health = self.maxhealth;
    self iPrintLn("^2Revived");
}

doMaxAmmo()
{
    w = self getWeaponsList();
    for (i = 0; i < w.size; i++)
        self giveMaxAmmo(w[i]);
    self iPrintLn("^2Max Ammo");
}

doGive(name)
{
    w = self getWeaponsList();
    if (w.size >= 2)
        self takeWeapon(self getCurrentWeapon());
    self giveWeapon(name);
    self switchToWeapon(name);
    self giveMaxAmmo(name);
    self iPrintLn("^2" + name);
}

doPerks()
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
    p[p.size] = "specialty_grenadepulldeath";
    p[p.size] = "specialty_otherapparition";
    for (i = 0; i < p.size; i++)
        if (!self hasPerk(p[i]))
            self setPerk(p[i]);
    self.num_perks = p.size;
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
    self iPrintLn("^2Killed " + c + " zombies");
}

// freezeControls is player-only; g_ai pauses all AI thinking instead.
doFreeze()
{
    self.mn.frozen = !self.mn.frozen;
    if (self.mn.frozen)
        setDvar("g_ai", "0");
    else
        setDvar("g_ai", "1");
    self iPrintLn("Freeze: " + tf(self.mn.frozen));
}

doOpenDoors()
{
    ents = getEntArray("zombie_door", "targetname");
    for (i = 0; i < ents.size; i++)
        if (isDefined(ents[i]))
            ents[i] notify("trigger", self);
    ents = getEntArray("zombie_debris", "targetname");
    for (i = 0; i < ents.size; i++)
        if (isDefined(ents[i]))
            ents[i] notify("trigger", self);
    self iPrintLn("^2All Doors Opened");
}

ammoLoop()
{
    self endon("disconnect");
    level endon("game_ended");
    for (;;)
    {
        wait 0.1;
        if (isDefined(self.mn) && isDefined(self.mn.infammo) && self.mn.infammo)
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
