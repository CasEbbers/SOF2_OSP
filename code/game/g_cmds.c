// Copyright (C) 2001-2002 Raven Software.
//
#include "g_local.h"

#include "../../ui/menudef.h"

int AcceptBotCommand(char *cmd, gentity_t *pl);

/*
==================
DeathmatchScoreboardMessage
==================
*/
void DeathmatchScoreboardMessage(gentity_t *ent)
{
    char entry[1024];
    char string[1400];
    int stringlength;
    int i, j;
    gclient_t *cl;
    int numSorted;

    // send the latest information on all clients
    string[0] = 0;
    stringlength = 0;

    numSorted = level.numConnectedClients;

    for (i = 0; i < numSorted; i++)
    {
        int ping;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == CON_CONNECTING)
        {
            ping = -1;
        }
        else
        {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        Com_sprintf(entry, sizeof(entry),
                    " %i %i %i %i %i %i %i %i %i",
                    level.sortedClients[i],
                    // OSP
                    //cl->sess.score,
                    (g_alternateSort.integer == 0)
                        ? (cl->sess.score)
                        : ((g_alternateSort.integer == 1)
                               ? (int)(OSP_GetKDR(cl) * 100)
                               : (int)(OSP_GetKPM(cl) * 3600)),
                    // END
                    cl->sess.kills,
                    cl->sess.deaths,
                    ping,
                    (level.time - cl->pers.enterTime) / 60000,
                    (cl->sess.ghost || cl->ps.pm_type == PM_DEAD) ? qtrue : qfalse,
                    g_entities[level.sortedClients[i]].s.gametypeitems,
                    g_teamkillDamageMax.integer ? 100 * cl->sess.teamkillDamage / g_teamkillDamageMax.integer : 0);

        j = strlen(entry);
        if (stringlength + j > 1022)
        {
            break;
        }

        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap_SendServerCommand(ent - g_entities, va("scores %i %i %i%s", i,
                                                level.teamScores[TEAM_RED],
                                                level.teamScores[TEAM_BLUE],
                                                string));
}

/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f(gentity_t *ent)
{
    DeathmatchScoreboardMessage(ent);
}

/*
==================
CheatsOk
==================
*/
qboolean CheatsOk(gentity_t *ent)
{
    if (!g_cheats.integer)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"Cheats are NOT enabled on this server.\n\""));
        return qfalse;
    }
    if (ent->health <= 0)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"You must be alive to use this command.\n\""));
        return qfalse;
    }
    return qtrue;
}

/*
==================
ConcatArgs
==================
*/
char *ConcatArgs(int start)
{
    int i, c, tlen;
    static char line[MAX_STRING_CHARS];
    int len;
    char arg[MAX_STRING_CHARS];

    len = 0;
    c = trap_Argc();
    for (i = start; i < c; i++)
    {
        trap_Argv(i, arg, sizeof(arg));
        tlen = strlen(arg);
        if (len + tlen >= MAX_STRING_CHARS - 1)
        {
            break;
        }
        memcpy(line + len, arg, tlen);
        len += tlen;
        if (i != c - 1)
        {
            line[len] = ' ';
            len++;
        }
    }

    line[len] = 0;

    return line;
}

/*
==================
SanitizeString

Remove case and control characters
==================
*/
void SanitizeString(char *in, char *out)
{
    while (*in)
    {
        if (*in == 27)
        {
            in += 2; // skip color code
            continue;
        }
        if (*in < 32)
        {
            in++;
            continue;
        }
        *out++ = tolower(*in++);
    }

    *out = 0;
}

/*
==================
G_ClientNumberFromName

Finds the client number of the client with the given name
==================
*/
int G_ClientNumberFromName(const char *name)
{
    char s2[MAX_STRING_CHARS];
    char n2[MAX_STRING_CHARS];
    int i;
    gclient_t *cl;

    // check for a name match
    SanitizeString((char *)name, s2);
    for (i = 0, cl = level.clients; i < level.numConnectedClients; i++, cl++)
    {
        SanitizeString(cl->pers.netname, n2);
        if (!strcmp(n2, s2))
        {
            return i;
        }
    }

    return -1;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString(gentity_t *to, char *s)
{
    gclient_t *cl;
    int idnum;
    char s2[MAX_STRING_CHARS];
    char n2[MAX_STRING_CHARS];

    // numeric values are just slot numbers
    if (s[0] >= '0' && s[0] <= '9')
    {
        idnum = atoi(s);
        if (idnum < 0 || idnum >= level.maxclients)
        {
            trap_SendServerCommand(to - g_entities, va("print \"Bad client slot: %i\n\"", idnum));
            return -1;
        }

        cl = &level.clients[idnum];
        if (cl->pers.connected != CON_CONNECTED)
        {
            trap_SendServerCommand(to - g_entities, va("print \"Client %i is not active\n\"", idnum));
            return -1;
        }
        return idnum;
    }

    // check for a name match
    SanitizeString(s, s2);
    for (idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++)
    {
        if (cl->pers.connected != CON_CONNECTED)
        {
            continue;
        }
        SanitizeString(cl->pers.netname, n2);
        if (!strcmp(n2, s2))
        {
            return idnum;
        }
    }

    trap_SendServerCommand(to - g_entities, va("print \"User %s is not on the server\n\"", s));
    return -1;
}

/*
==================
Cmd_Drop_f

Drops the currenty selected weapon
==================
*/
void Cmd_Drop_f(gentity_t *ent)
{
    gentity_t *dropped;

    // spectators cant drop anything since they dont have anything
    if (ent->client->sess.team == TEAM_SPECTATOR)
    {
        return;
    }

    // Ghosts and followers cant drop stuff
    if (ent->client->ps.pm_flags & (PMF_GHOST | PMF_FOLLOW))
    {
        return;
    }

    // Drop the weapon the client wanted to drop
    dropped = G_DropWeapon(ent, atoi(ConcatArgs(1)), 3000);
    if (!dropped)
    {
        return;
    }
}

/*
==================
Cmd_DropItem_f

Drops the gametype items the player is carrying
==================
*/
void Cmd_DropItem_f(gentity_t *ent)
{
    // spectators cant drop anything since they dont have anything
    if (ent->client->sess.team == TEAM_SPECTATOR)
    {
        return;
    }

    // Ghosts and followers cant drop stuff
    if (ent->client->ps.pm_flags & (PMF_GHOST | PMF_FOLLOW))
    {
        return;
    }

    // Nothing to drop
    if (!ent->client->ps.stats[STAT_GAMETYPE_ITEMS])
    {
        return;
    }

    // OSP
    if (strcmp(level.gametypeData->name, "vip") == 0)
    {
        return;
    }
    // END

    G_DropGametypeItems(ent, 3000);
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f(gentity_t *ent)
{
    char *name;
    gitem_t *it;
    int i;
    qboolean give_all;
    gentity_t *it_ent;
    trace_t trace;
    char arg[MAX_QPATH];

    int start;
    int end;
    int l;

    trap_Argv(1, arg, sizeof(arg));

    if (!Q_stricmp(arg, "me"))
    {
        start = ent->s.number;
        end = start + 1;
    }
    else if (!Q_stricmp(arg, "all"))
    {
        start = 0;
        end = MAX_CLIENTS;
    }
    else
    {
        start = atoi(arg);
        end = start + 1;
    }

    for (l = start; l < end; l++)
    {
        ent = &g_entities[l];

        if (!ent->inuse)
        {
            continue;
        }

        if (G_IsClientDead(ent->client))
        {
            continue;
        }

        if (!CheatsOk(ent))
        {
            return;
        }

        name = ConcatArgs(2);

        if (Q_stricmp(name, "all") == 0)
            give_all = qtrue;
        else
            give_all = qfalse;

        if (give_all || Q_stricmp(name, "health") == 0)
        {
            ent->health = MAX_HEALTH;
            if (!give_all)
                continue;
        }

        if (give_all || Q_stricmp(name, "weapons") == 0)
        {
            ent->client->ps.stats[STAT_WEAPONS] = (1 << WP_NUM_WEAPONS) - 1 - (1 << WP_NONE);
            if (!give_all)
                continue;
        }

        if (give_all || Q_stricmp(name, "ammo") == 0)
        {
            for (i = WP_NONE + 1; i < WP_NUM_WEAPONS; i++)
            {
                attackType_t a;

                for (a = ATTACK_NORMAL; a < ATTACK_MAX; a++)
                {
                    ent->client->ps.clip[a][i] = weaponData[i].attack[a].clipSize;
                    // OSP
                    //ent->client->ps.ammo[weaponData[i].attack[a].ammoIndex] = ammoData[weaponData[i].attack[a].ammoIndex].max;
                    (weaponData[i].attack[a].ammoIndex < MAX_AMMO)
                        ? (ent->client->ps.ammo[weaponData[i].attack[a].ammoIndex] = ammoData[weaponData[i].attack[a].ammoIndex].max)
                        : (ent->client->ps.firemode[MAX_WEAPONS - 1 - (weaponData[i].attack[a].ammoIndex - MAX_AMMO)] = ammoData[weaponData[i].attack[a].ammoIndex].max);
                    // END
                }
            }

            if (!give_all)
                continue;
        }

        if (give_all || Q_stricmp(name, "armor") == 0)
        {
            ent->client->ps.stats[STAT_ARMOR] = MAX_ARMOR;

            if (!give_all)
                continue;
        }

        // spawn a specific item right on the player
        if (!give_all)
        {
            it = BG_FindItem(name);
            if (!it)
            {
                continue;
            }

            if (it->giType == IT_GAMETYPE)
            {
                continue;
            }

            it_ent = G_Spawn();
            VectorCopy(ent->r.currentOrigin, it_ent->s.origin);
            it_ent->classname = it->classname;
            G_SpawnItem(it_ent, it);
            FinishSpawningItem(it_ent);
            memset(&trace, 0, sizeof(trace));
            Touch_Item(it_ent, ent, &trace);
            if (it_ent->inuse)
            {
                G_FreeEntity(it_ent);
            }
        }
    }
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f(gentity_t *ent)
{
    char *msg;

    if (!CheatsOk(ent))
    {
        return;
    }

    ent->flags ^= FL_GODMODE;
    if (!(ent->flags & FL_GODMODE))
        msg = "godmode OFF\n";
    else
        msg = "godmode ON\n";

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f(gentity_t *ent)
{
    char *msg;

    if (!CheatsOk(ent))
    {
        return;
    }

    ent->flags ^= FL_NOTARGET;
    if (!(ent->flags & FL_NOTARGET))
        msg = "notarget OFF\n";
    else
        msg = "notarget ON\n";

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f(gentity_t *ent)
{
    char *msg;

    if (!CheatsOk(ent))
    {
        return;
    }

    if (ent->client->noclip)
    {
        msg = "noclip OFF\n";
    }
    else
    {
        msg = "noclip ON\n";
    }
    ent->client->noclip = !ent->client->noclip;

    trap_SendServerCommand(ent - g_entities, va("print \"%s\"", msg));
}

/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f(gentity_t *ent)
{
    if (!CheatsOk(ent))
    {
        return;
    }

    BeginIntermission();

    trap_SendServerCommand(ent - g_entities, "clientLevelShot");
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f(gentity_t *ent)
{
    // No killing yourself if your a spectator
    if (G_IsClientSpectating(ent->client))
    {
        // OSP
        Com_Printf("You must be a player to use /kill\n");
        // END
        return;
    }

    // No killing yourself if your dead
    if (G_IsClientDead(ent->client))
    {
        // OSP
        Com_Printf("You must be alive to use /kill\n");
        // END
        return;
    }

    ent->flags &= ~FL_GODMODE;
    ent->client->ps.stats[STAT_HEALTH] = ent->health = -999;
    player_die(ent, ent, ent, 100000, MOD_SUICIDE, HL_NONE, vec3_origin);
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gclient_t *client, int oldTeam)
{
    switch (client->sess.team)
    {
    case TEAM_RED:
        // OSP
        //trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the red team.\n\"", client->pers.netname));
        if (strcmp(team_redName.string, "Red team") == 0)
            trap_SendServerCommand(-1, va("cp \"%s^7 joined the red team.\n\"", client->pers.netname));
        else
            trap_SendServerCommand(-1, va("cp \"%s^7 joined %s (red).\n\"", client->pers.netname, team_redName.string));
        // END
        break;

    case TEAM_BLUE:
        // OSP
        //trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the blue team.\n\"", client->pers.netname));
        if (strcmp(team_blueName.string, "Blue team") == 0)
            trap_SendServerCommand(-1, va("cp \"%s^7 joined the blue team.\n\"", client->pers.netname));
        else
            trap_SendServerCommand(-1, va("cp \"%s^7 joined %s (blue).\n\"", client->pers.netname, team_blueName.string));
        // END
        break;

    case TEAM_SPECTATOR:
        if (oldTeam != TEAM_SPECTATOR)
        {
            trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the spectators.\n\"", client->pers.netname));
        }
        break;

    case TEAM_FREE:
        trap_SendServerCommand(-1, va("cp \"%s" S_COLOR_WHITE " joined the battle.\n\"", client->pers.netname));
        break;
    }
}

/*
=================
SetTeam
=================
*/
// OSP
//void SetTeam( gentity_t *ent, char *s, const char* identity )
void SetTeam(gentity_t *ent, char *s, const char *identity, qboolean force)
// END
{
    int team;
    int oldTeam;
    gclient_t *client;
    int clientNum;
    spectatorState_t specState;
    int specClient;
    qboolean ghost;
    qboolean noOutfittingChange = qfalse;

    // see what change is requested
    //
    client = ent->client;

    clientNum = client - level.clients;
    specClient = 0;
    specState = SPECTATOR_NOT;

    // If an identity was specified then inject it into
    // the clients userinfo
    if (identity)
    {
        char userinfo[MAX_INFO_STRING];
        trap_GetUserinfo(clientNum, userinfo, sizeof(userinfo));

        if (Q_stricmp(identity, Info_ValueForKey(userinfo, "identity")))
        {
            Info_SetValueForKey(userinfo, "identity", identity);
            Info_SetValueForKey(userinfo, "team_identity", identity);
            trap_SetUserinfo(clientNum, userinfo);
        }
        else
        {
            identity = NULL;
        }
    }

    if (!Q_stricmp(s, "follow1"))
    {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FOLLOW;
        specClient = -1;
    }
    else if (!Q_stricmp(s, "follow2"))
    {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FOLLOW;
        specClient = -2;
    }
    else if (!Q_stricmp(s, "spectator") || !Q_stricmp(s, "s"))
    {
        team = TEAM_SPECTATOR;
        specState = SPECTATOR_FREE;
    }
    else if (level.gametypeData->teams)
    {
        // if running a team game, assign player to one of the teams
        specState = SPECTATOR_NOT;
        if (!Q_stricmp(s, "red") || !Q_stricmp(s, "r"))
        {
            team = TEAM_RED;
        }
        else if (!Q_stricmp(s, "blue") || !Q_stricmp(s, "b"))
        {
            team = TEAM_BLUE;
        }
        else
        {
            // pick the team with the least number of players
            team = PickTeam(clientNum);
        }

        if (g_teamForceBalance.integer)
        {
            int counts[TEAM_NUM_TEAMS];

            counts[TEAM_BLUE] = TeamCount(ent->client->ps.clientNum, TEAM_BLUE, NULL);
            counts[TEAM_RED] = TeamCount(ent->client->ps.clientNum, TEAM_RED, NULL);

            // We allow a spread of two
            if (team == TEAM_RED && counts[TEAM_RED] - counts[TEAM_BLUE] > 1)
            {
                trap_SendServerCommand(ent->client->ps.clientNum,
                                       "cp \"Red team has too many players.\n\"");

                // ignore the request
                return;
            }
            if (team == TEAM_BLUE && counts[TEAM_BLUE] - counts[TEAM_RED] > 1)
            {
                trap_SendServerCommand(ent->client->ps.clientNum,
                                       "cp \"Blue team has too many players.\n\"");

                // ignore the request
                return;
            }

            // It's ok, the team we are switching to has less or same number of players
        }
    }
    else
    {
        // force them to spectators if there aren't any spots free
        team = TEAM_FREE;
    }

    // override decision if limiting the players
    if (g_maxGameClients.integer > 0 && level.numNonSpectatorClients >= g_maxGameClients.integer)
    {
        team = TEAM_SPECTATOR;
    }

    // decide if we will allow the change
    oldTeam = client->sess.team;
    ghost = client->sess.ghost;

    if (team == oldTeam && team != TEAM_SPECTATOR)
    {
        if (identity)
        {
            // get and distribute relevent paramters
            client->pers.identity = NULL;
            ClientUserinfoChanged(clientNum);
        }

        return;
    }

    // OSP
    if (!OSP_TeamchangeAllowed(team, oldTeam, ent) && !force)
    {
        return;
    }

    ent->client->ospClient->x74 = 0;
    // END

    noOutfittingChange = ent->client->noOutfittingChange;

    // he starts at 'base'
    client->pers.teamState.state = TEAM_BEGIN;

    if (oldTeam != TEAM_SPECTATOR)
    {
        if (ghost)
        {
            G_StopGhosting(ent);
        }
        else if (!G_IsClientDead(client))
        {
            // Kill him (makes sure he loses flags, etc)
            ent->flags &= ~FL_GODMODE;
            ent->client->ps.stats[STAT_HEALTH] = ent->health = 0;
            player_die(ent, ent, ent, 100000, MOD_TEAMCHANGE, HL_NONE, vec3_origin);

            ent->client->sess.ghost = qfalse;
        }
    }

    // If respawn interval start as a ghost
    if (level.gametypeRespawnTime[team])
    {
        ghost = qtrue;
    }

    // they go to the end of the line
    if (team == TEAM_SPECTATOR)
    {
        client->sess.spectatorTime = level.time;
    }

    client->sess.team = team;
    client->sess.spectatorState = specState;
    client->sess.spectatorClient = specClient;

    // Kill any child entities of this client to protect against grenade team changers
    G_FreeEnitityChildren(ent);

    // Always spawn into a ctf game using a respawn timer.
    if (team != TEAM_SPECTATOR && level.gametypeData->respawnType == RT_INTERVAL)
    {
        G_SetRespawnTimer(ent);
        ghost = qtrue;
    }

    // OSP
    if (client->ospClient->jailTime && client->ospClient->jailTime > level.time)
    {
        ent->client->ps.respawnTimer = ent->client->ospClient->jailTime + 1000;
        ghost = qtrue;
    }
    // END

    BroadcastTeamChange(client, oldTeam);

    // See if we should spawn as a ghost
    if (team != TEAM_SPECTATOR && level.gametypeData->respawnType == RT_NONE)
    {
        // If there are ghosts already then spawn as a ghost because
        // the game is already in progress.
        if (!level.warmupTime && (level.gametypeJoinTime && (level.time - level.gametypeJoinTime) > (g_roundjointime.integer * 1000)) || noOutfittingChange || client->sess.noTeamChange)
        {
            ghost = qtrue;
        }

        // Spectator to a team doesnt count
        if (oldTeam != TEAM_SPECTATOR)
        {
            client->sess.noTeamChange = qtrue;
        }
    }

    // If a ghost, enforce it
    if (ghost)
    {
        // Make them a ghost again
        if (team != TEAM_SPECTATOR)
        {
            G_StartGhosting(ent);

            // get and distribute relevent paramters
            client->pers.identity = NULL;
            ClientUserinfoChanged(clientNum);

            CalculateRanks();

            return;
        }
    }

    // get and distribute relevent paramters
    client->pers.identity = NULL;
    ClientUserinfoChanged(clientNum);

    CalculateRanks();

    // Begin the clients new life on the their new team
    ClientBegin(clientNum);
}

/*
=================
G_StartGhosting

Starts a client ghosting.  This essentially will kill a player which is alive
=================
*/
void G_StartGhosting(gentity_t *ent)
{
    int i;

    // Dont start ghosting if already ghosting
    if (ent->client->sess.ghost)
    {
        return;
    }

    ent->client->sess.ghost = qtrue;
    ent->client->sess.spectatorState = SPECTATOR_FREE;
    ent->client->sess.spectatorClient = -1;
    ent->client->ps.pm_flags |= PMF_GHOST;
    ent->client->ps.stats[STAT_HEALTH] = 100;
    ent->client->ps.pm_type = PM_SPECTATOR;
    ent->client->ps.pm_flags &= ~PMF_FOLLOW;

    trap_UnlinkEntity(ent);

    // stop any following clients
    for (i = 0; i < level.maxclients; i++)
    {
        if (G_IsClientSpectating(&level.clients[i]) && level.clients[i].sess.spectatorState == SPECTATOR_FOLLOW && level.clients[i].sess.spectatorClient == ent->s.number)
        {
            G_StopFollowing(&g_entities[i]);

            // OSP
            if (OSP_IsClientAdmin(i, qtrue))
            {
                Cmd_FollowCycle_f(&g_entities[i], 1);
            }
            // END
        }
    }
}

/*
=================
G_StopGhosting

Stops a client from ghosting.  The client will be dead after this
call
=================
*/
void G_StopGhosting(gentity_t *ent)
{
    // Dont stop someone who isnt ghosting in the first place
    if (!ent->client->sess.ghost)
    {
        return;
    }

    ent->client->sess.ghost = qfalse;
    ent->client->ps.pm_flags &= ~PMF_GHOST;
    ent->client->ps.pm_flags &= ~PMF_FOLLOW;

    if (ent->client->sess.team == TEAM_SPECTATOR)
    {
        ent->client->ps.pm_type = PM_SPECTATOR;
    }
    else
    {
        ent->client->ps.pm_type = PM_DEAD;
        ent->health = ent->client->ps.stats[STAT_HEALTH] = 0;
    }
}

/*
=================
G_StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
void G_StopFollowing(gentity_t *ent)
{
    // OSP
    ent->client->ospClient->x74 = 0;
    // END

    // Cant stop following if not following in the first place
    if (!(ent->client->ps.pm_flags & PMF_FOLLOW))
    {
        return;
    }

    // Clear the following variables
    ent->client->ps.pm_flags &= ~PMF_FOLLOW;
    ent->client->sess.spectatorState = SPECTATOR_FREE;
    ent->client->ps.clientNum = ent - g_entities;
    ent->client->ps.zoomFov = 0;
    ent->client->ps.loopSound = 0;
    ent->client->ps.pm_flags &= ~(PMF_GOGGLES_ON | PMF_ZOOM_FLAGS);
    ent->client->ps.persistant[PERS_TEAM] = ent->client->sess.team;
    ent->r.svFlags &= ~SVF_BOT;

    // If we were in fact following someone, then make the angles and origin nice for
    // when we stop
    if (ent->client->sess.spectatorClient != -1)
    {
        gclient_t *cl = &level.clients[ent->client->sess.spectatorClient];

        int i;
        for (i = 0; i < 3; i++)
        {
            ent->client->ps.delta_angles[i] = ANGLE2SHORT(cl->ps.viewangles[i] - SHORT2ANGLE(ent->client->pers.cmd.angles[i]));
        }

        VectorCopy(cl->ps.viewangles, ent->client->ps.viewangles);
        VectorCopy(cl->ps.origin, ent->client->ps.origin);
        VectorClear(ent->client->ps.velocity);
        ent->client->ps.movementDir = 0;

        BG_PlayerStateToEntityState(&ent->client->ps, &ent->s, qtrue);
    }

    // Ghots dont really become spectators, just psuedo spectators
    if (ent->client->sess.ghost)
    {
        // Do a start and stop to ensure the variables are all set properly
        G_StopGhosting(ent);
        G_StartGhosting(ent);
    }
    else
    {
        ent->client->sess.team = TEAM_SPECTATOR;
        ent->client->ps.persistant[PERS_TEAM] = TEAM_SPECTATOR;
    }
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f(gentity_t *ent)
{
    char team[MAX_TOKEN_CHARS];
    char identity[MAX_TOKEN_CHARS];

    // Need at least the team specified in the arguments
    if (trap_Argc() < 2)
    {
        int oldTeam = ent->client->sess.team;
        switch (oldTeam)
        {
        case TEAM_BLUE:
            trap_SendServerCommand(ent - g_entities, "print \"Blue team\n\"");
            break;

        case TEAM_RED:
            trap_SendServerCommand(ent - g_entities, "print \"Red team\n\"");
            break;

        case TEAM_FREE:
            trap_SendServerCommand(ent - g_entities, "print \"Free team\n\"");
            break;

        case TEAM_SPECTATOR:
            trap_SendServerCommand(ent - g_entities, "print \"Spectator team\n\"");
            break;
        }

        return;
    }

    // Limit how often one can switch team
    if (ent->client->switchTeamTime > level.time)
    {
        trap_SendServerCommand(ent - g_entities, "print \"May not switch teams more than once per 5 seconds.\n\"");
        return;
    }

    trap_Argv(1, team, sizeof(team));
    trap_Argv(2, identity, sizeof(identity));

    // OSP
    //SetTeam(ent, team, identity[0] ? identity : NULL);
    SetTeam(ent, team, identity[0] ? identity : NULL, qfalse);
    // END

    // Remember the team switch time so they cant do it again really quick
    ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f(gentity_t *ent)
{
    int i;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() != 2)
    {
        if (ent->client->sess.spectatorState == SPECTATOR_FOLLOW)
        {
            G_StopFollowing(ent);
        }

        // OSP
        ent->client->ospClient->x38 = 0;
        // END

        return;
    }

    trap_Argv(1, arg, sizeof(arg));

    // OSP
    if (arg[0] == 'r' || strcmp("red", arg) == 0)
    {
        ent->client->ospClient->x38 = TEAM_RED;
        Cmd_FollowCycle_f(ent, 1);
        return;
    }
    else if (arg[0] == 'b' || strcmp("blue", arg) == 0)
    {
        ent->client->ospClient->x38 = TEAM_BLUE;
        Cmd_FollowCycle_f(ent, 1);
        return;
    }
    else
    {
        ent->client->ospClient->x38 = 0;
    }
    // END

    i = ClientNumberFromString(ent, arg);
    if (i == -1)
    {
        return;
    }

    // can't follow self
    if (&level.clients[i] == ent->client)
    {
        return;
    }

    // cant cycle to dead people
    if (level.clients[i].ps.pm_type == PM_DEAD)
    {
        return;
    }

    // can't follow another spectator
    if (G_IsClientSpectating(&level.clients[i]))
    {
        return;
    }

    // Dissallow following of the enemy if the cvar is set
    // OSP
    //if (level.gametypeData->teams && !g_followEnemy.integer && ent->client->sess.team != TEAM_SPECTATOR)
    if (level.gametypeData->teams && !g_followEnemy.integer && !OSP_IsClientAdmin(ent - g_entities, qtrue) && ent->client->sess.team != TEAM_SPECTATOR)
    // END
    {
        // Are they on the same team?
        if (level.clients[i].sess.team != ent->client->sess.team)
        {
            return;
        }
    }

    // OSP
    if (!OSP_CanFollow(ent, level.clients[i].sess.team))
    {
        return;
    }
    // END

    // first set them to spectator as long as they arent a ghost
    if (!ent->client->sess.ghost && ent->client->sess.team != TEAM_SPECTATOR)
    {
        // OSP
        //SetTeam( ent, "spectator", NULL );
        SetTeam(ent, "spectator", NULL, qfalse);
        // END
    }

    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f(gentity_t *ent, int dir)
{
    int clientnum;
    int deadclient;
    int original;

    // first set them to spectator
    if (!ent->client->sess.ghost && ent->client->sess.team != TEAM_SPECTATOR)
    {
        // OSP
        // SetTeam( ent, "spectator", NULL );
        SetTeam(ent, "spectator", NULL, qfalse);
        // END
    }

    if (dir != 1 && dir != -1)
    {
        Com_Error(ERR_FATAL, "Cmd_FollowCycle_f: bad dir %i", dir);
    }

    if (ent->client->sess.spectatorClient == -1)
    {
        clientnum = original = ent->s.number;
    }
    else
    {
        clientnum = original = ent->client->sess.spectatorClient;
    }

    // OSP
    if (g_forceFollow.integer > 1)
    {
        ent->client->sess.spectatorFirstPerson = qtrue;
    }
    // END

    deadclient = -1;
    do
    {
        clientnum += dir;
        if (clientnum >= level.maxclients)
        {
            clientnum = 0;
        }
        if (clientnum < 0)
        {
            clientnum = level.maxclients - 1;
        }

        // can only follow connected clients
        if (level.clients[clientnum].pers.connected != CON_CONNECTED)
        {
            continue;
        }

        // can't follow another spectator
        if (G_IsClientSpectating(&level.clients[clientnum]))
        {
            continue;
        }

        // Cant switch to dead people unless there is nobody else to switch to
        if (G_IsClientDead(&level.clients[clientnum]))
        {
            deadclient = clientnum;
            continue;
        }

        // OSP
        if (level.gametypeData->teams &&
            ent->client->sess.team == TEAM_SPECTATOR &&
            ent->client->ospClient->x38 &&
            ent->client->ospClient->x38 != level.clients[clientnum].sess.team)
        {
            continue;
        }
        // END

        // Dissallow following of the enemy if the cvar is set
        // OSP
        //if (level.gametypeData->teams && !g_followEnemy.integer && ent->client->sess.team != TEAM_SPECTATOR)
        if (level.gametypeData->teams && !g_followEnemy.integer && !OSP_IsClientAdmin(ent - g_entities, qtrue) && ent->client->sess.team != TEAM_SPECTATOR)
        // END
        {
            // Are they on the same team?
            if (level.clients[clientnum].sess.team != ent->client->sess.team)
            {
                continue;
            }
        }

        // OSP
        if (!OSP_CanFollow(ent, level.clients[clientnum].sess.team))
        {
            continue;
        }
        // END

        // this is good, we can use it)
        ent->client->sess.spectatorClient = clientnum;
        ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
        return;

    } while (clientnum != original);

    // If being forced to follow and there is a dead client to jump to, then jump to them now
    // OSP
    //if (deadclient != -1 && g_forceFollow.integer)
    //{
    //    // this is good, we can use it
    //    ent->client->sess.spectatorClient = deadclient;
    //    ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
    //    return;
    //}
    if (ent->client->ospClient->x74)
    {
    }
    else
    {
        if (deadclient != -1 && g_forceFollow.integer && !OSP_IsClientAdmin(ent - g_entities, qtrue))
        {
            // this is good, we can use it
            ent->client->sess.spectatorClient = deadclient;
            ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
            return;
        }
    }
    // END

    G_StopFollowing(ent);

    // leave it where it was
}

/*
==================
G_SayTo
==================
*/
static void G_SayTo(gentity_t *ent, gentity_t *other, int mode, const char *name, const char *message)
{
    qboolean ghost = qfalse;
    qboolean spec = qfalse;
    const char *type;

    if (!other)
    {
        return;
    }

    if (!other->inuse)
    {
        return;
    }

    if (!other->client)
    {
        return;
    }

    if (other->client->pers.connected != CON_CONNECTED)
    {
        return;
    }

    if (mode == SAY_TEAM && !OnSameTeam(ent, other))
    {
        return;
    }

    // OSP
    if (match_mutespecs.integer > 0 &&
        (!(ent->client->sess.team != TEAM_FREE || other->client->sess.team == TEAM_FREE) ||
         !(ent->client->sess.team != TEAM_SPECTATOR || other->client->sess.team == TEAM_SPECTATOR)))
    {
        return;
    }
    // END

    if (ent->client->sess.muted || G_IsClientChatIgnored(other->s.number, ent->s.number))
    {
        return;
    }

    // OSP
    //if (!level.intermissiontime && !level.intermissionQueued)
    if (!level.intermissiontime && !level.intermissionQueued && osp.matchPhase >= 3 && !g_voiceTalkingGhosts.integer)
    // END
    {
        // Spectators cant talk to alive people
        if (ent->client->sess.team == TEAM_SPECTATOR)
        {
            spec = qtrue;
        }

        if (level.gametypeData->respawnType == RT_NONE)
        {
            // Dead people cant talk to alive people
            if (!spec && G_IsClientDead(ent->client))
            {
                ghost = qtrue;
            }

            // If the client we are talking to is alive then a check
            // must be made to see if this talker is alowed to speak to this person
            if (ent->s.number != other->s.number && !G_IsClientDead(other->client) && !G_IsClientSpectating(other->client) && (ghost || spec))
            {
                return;
            }
        }
    }
    // OSP
    else if (g_voiceTalkingGhosts.integer && !level.intermissiontime && !level.intermissionQueued)
    {
        if (ent->client->sess.team == TEAM_SPECTATOR)
        {
            spec = qtrue;
        }

        if (level.gametypeData->respawnType == RT_NONE)
            if (!spec && G_IsClientDead(ent->client))
                ghost = qtrue;

        if (g_voiceTalkingGhosts.integer == 1 && ent->s.number != other->s.number && !G_IsClientDead(other->client) && !G_IsClientSpectating(other->client) && ghost && mode)
        {
            return;
        }
    }
    // END

    type = "";
    if (ghost)
    {
        type = "*ghost* ";
    }
    else if (spec)
    {
        type = "*spec* ";
    }

    trap_SendServerCommand(other - g_entities, va("%s %d \"%s%s%s\"",
                                                  mode == SAY_TEAM ? "tchat" : "chat",
                                                  ent->s.number,
                                                  type, name, message));
}

/*
==================
G_GetChatPrefix
==================
*/
void G_GetChatPrefix(gentity_t *ent, gentity_t *target, int mode, char *name, int nameSize)
{
    const char *namecolor;
    char location[64];
    qboolean locationOk = qtrue;

    // Spectators and ghosts dont show locations
    if (ent->client->ps.pm_type == PM_DEAD || G_IsClientSpectating(ent->client))
    {
        locationOk = qfalse;
    }

    if (!level.gametypeData->teams && mode == SAY_TEAM)
    {
        mode = SAY_ALL;
    }

    if (level.gametypeData->teams)
    {
        switch (ent->client->sess.team)
        {
        case TEAM_BLUE:
            namecolor = S_COLOR_BLUE;
            break;
        case TEAM_RED:
            namecolor = S_COLOR_RED;
            break;
        default:
            namecolor = S_COLOR_WHITE;
            break;
        }
    }
    else
    {
        namecolor = S_COLOR_WHITE;
    }

    switch (mode)
    {
    default:
    case SAY_ALL:
        Com_sprintf(name, nameSize, "%s%s%s: ", namecolor, ent->client->pers.netname, S_COLOR_WHITE);
        break;

    // OSP
    case SAY_TEAM_NL:
        if (ent->client->sess.team == TEAM_RED)
        {
            Com_sprintf(name, nameSize, "^3((%s%s^3))%s: ",
                        namecolor,
                        ent->client->pers.netname,
                        S_COLOR_WHITE);
        }
        else if (ent->client->sess.team == TEAM_BLUE)
        {
            Com_sprintf(name, nameSize, "^5((%s%s^5))%s: ",
                        namecolor,
                        ent->client->pers.netname,
                        S_COLOR_WHITE);
        }
        else
        {
            Com_sprintf(name, nameSize, "((%s%s%s)): ",
                        namecolor,
                        ent->client->pers.netname,
                        S_COLOR_WHITE);
        }
        break;
        // END

    case SAY_TEAM:
        if (locationOk && Team_GetLocationMsg(ent, location, sizeof(location)))
        {
            Com_sprintf(name, nameSize, "%s(%s%s) %s(%s): ",
                        namecolor,
                        ent->client->pers.netname,
                        namecolor,
                        S_COLOR_WHITE, location);
        }
        else
        {
            Com_sprintf(name, nameSize, "%s(%s%s)%s: ",
                        namecolor,
                        ent->client->pers.netname,
                        namecolor,
                        S_COLOR_WHITE);
        }
        break;

    case SAY_TELL:
        if (locationOk && target && level.gametypeData->teams &&
            target->client->sess.team == ent->client->sess.team &&
            Team_GetLocationMsg(ent, location, sizeof(location)))
        {
            Com_sprintf(name, nameSize, "%s[%s%s] %s(%s): ",
                        namecolor,
                        ent->client->pers.netname,
                        namecolor,
                        S_COLOR_WHITE, location);
        }
        else
        {
            Com_sprintf(name, nameSize, "%s[%s%s]%s: ",
                        namecolor,
                        ent->client->pers.netname,
                        namecolor,
                        S_COLOR_WHITE);
        }
        break;
    }

    strcat(name, S_COLOR_GREEN);
}

/*
==================
G_Say
==================
*/
void G_Say(gentity_t *ent, gentity_t *target, int mode, const char *chatText)
{
    int j;
    gentity_t *other;
    char text[MAX_SAY_TEXT];
    char name[256];
    // OSP
    int len;

    len = strlen(chatText);

    if (len < 1)
    {
        return;
    }

    if (chatText[len - 1] == '^')
    {
        return;
    }
    // END

    // Logging stuff
    switch (mode)
    {
    case SAY_ALL:
        G_LogPrintf("say: %s: %s\n", ent->client->pers.netname, chatText);
        break;

    case SAY_TEAM:
        G_LogPrintf("sayteam: %s: %s\n", ent->client->pers.netname, chatText);
        break;

    // OSP
    case SAY_TEAM_NL:
        G_LogPrintf("sayteamnl: %s: %s\n", ent->client->pers.netname, chatText);
        break;
        // END
    }

    // Generate the chat prefix
    G_GetChatPrefix(ent, target, mode, name, sizeof(name));

    // Save off the chat text
    Q_strncpyz(text, chatText, sizeof(text));

    // OSP
    if (mode == SAY_TEAM_NL)
    {
        mode = SAY_TEAM;
    }

    if (!g_teamTokensOnly.integer || mode != SAY_ALL)
    {
        OSP_ParseTokens(ent, text, sizeof(text));
    }
    // END

    if (target)
    {
        G_SayTo(ent, target, mode, name, text);
        return;
    }

    // echo the text to the console
    if (g_dedicated.integer)
    {
        Com_Printf("%s%s\n", name, text);
    }

    // send it to all the apropriate clients
    for (j = 0; j < level.numConnectedClients; j++)
    {
        other = &g_entities[level.sortedClients[j]];
        G_SayTo(ent, other, mode, name, text);
    }
}

/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f(gentity_t *ent, int mode, qboolean arg0)
{
    char *p;

    if (trap_Argc() < 2 && !arg0)
    {
        return;
    }

    if (arg0)
    {
        p = ConcatArgs(0);
    }
    else
    {
        p = ConcatArgs(1);
    }

    // OSP
    //G_Say(ent, NULL, mode, p);
    if (sv_messageFiltering.integer)
    {
        if (OSP_FilterChattext(p) && sv_messageFiltering.integer == 2)
        {
            return;
        }
    }

    while (*p == ' ')
    {
        p++;
    }

    {
        int i;
        for (i = strlen(p) - 1; p[i] == ' ' && i >= 0; i--)
        {
            p[i] = '\0';
        }
    }

    if (OSP_IsClientAdmin(ent - g_entities, qfalse))
    {
        char *msg;

        if (p[0] == '/' &&
            tolower(p[1]) == 'a' &&
            tolower(p[2]) == 'c' &&
            p[3] != '\0' &&
            p[4] != '\0')
        {
            int i;

            msg = p + 4;

            for (i = 0; i < level.maxclients; i++)
            {
                if (!OSP_IsClientOnline(i))
                {
                    continue;
                }

                if (!OSP_IsClientAdmin(i, qfalse))
                {
                    continue;
                }

                trap_SendServerCommand(i, va("chat %d \"^6*admins only* ^5%s ^7: %s\"",
                                             ent->s.number,
                                             ent->client->pers.netname,
                                             msg));
            }

            G_LogPrintf("admin chat: %s: %s\n", ent->client->pers.netname, p + 4);
            return;
        }
        else if (p[0] == '/' &&
                 tolower(p[1]) == 'a' &&
                 tolower(p[2]) == 't' &&
                 p[3] != '\0' &&
                 p[4] != '\0')
        {
            msg = p + 4;
            trap_SendServerCommand(-1, va("chat %d \"^5*admin* ^7%s ^7: %s\"",
                                          ent->s.number,
                                          ent->client->pers.netname,
                                          msg));
            OSP_GlobalSound("sound/misc/menus/click.wav");
            G_LogPrintf("admin talk: %s: %s\n", ent->client->pers.netname, msg);
            return;
        }
    }

    ent->client->ospStats->numChats++;
    G_Say(ent, NULL, mode, p);
    // END
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f(gentity_t *ent)
{
    int targetNum;
    gentity_t *target;
    char *p;
    char arg[MAX_TOKEN_CHARS];

    if (trap_Argc() < 2)
    {
        return;
    }

    trap_Argv(1, arg, sizeof(arg));
    targetNum = atoi(arg);
    if (targetNum < 0 || targetNum >= level.maxclients)
    {
        return;
    }

    target = &g_entities[targetNum];
    if (!target || !target->inuse || !target->client)
    {
        return;
    }

    p = ConcatArgs(2);

    // OSP
    ent->client->ospStats->numChats++;
    // END

    G_LogPrintf("tell: %s to %s: %s\n", ent->client->pers.netname, target->client->pers.netname, p);
    G_Say(ent, target, SAY_TELL, p);
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !(ent->r.svFlags & SVF_BOT))
    {
        G_Say(ent, ent, SAY_TELL, p);
    }
}

static void G_VoiceTo(gentity_t *ent, gentity_t *other, int mode, const char *name, const char *id, qboolean voiceonly)
{
    // Only team say is supported right now for voice chatting
    if (mode != SAY_TEAM)
    {
        return;
    }

    if (!other || !other->inuse || !other->client)
    {
        return;
    }

    if (!OnSameTeam(ent, other))
    {
        return;
    }

    trap_SendServerCommand(other - g_entities, va("%s %d %d \"%s\" \"%s\"", "vtchat", voiceonly, ent->s.number, name, id));
}

/*
==================
G_CanVoiceGlobal

Can we globaly speak right now
==================
*/
qboolean G_CanVoiceGlobal(void)
{
    if (level.gametypeData->teams && level.time - level.globalVoiceTime > 5000)
    {
        return qtrue;
    }

    return qfalse;
}

/*
==================
G_VoiceGlobal

says something out loud that everyone in the radius can hear
==================
*/
void G_VoiceGlobal(gentity_t *ent, const char *id, qboolean force)
{
    if (!ent)
    {
        return;
    }

    if (!level.gametypeData->teams)
    {
        return;
    }

    if (!force && level.time - level.globalVoiceTime < 5000)
    {
        return;
    }

    level.globalVoiceTime = level.time;

    trap_SendServerCommand(-1, va("vglobal %d \"%s\"", ent->s.number, id));
}

/*
==================
G_Voice
==================
*/
void G_Voice(gentity_t *ent, gentity_t *target, int mode, const char *id, qboolean voiceonly)
{
    int j;
    gentity_t *other;
    char name[MAX_SAY_TEXT];

    // Spectators and ghosts dont talk
    if (ent->client->ps.pm_type == PM_DEAD || G_IsClientSpectating(ent->client))
    {
        return;
    }

    // Voice flooding protection on?
    if (g_voiceFloodCount.integer)
    {
        // If this client has been penalized for voice chatting to much then dont allow the voice chat
        if (ent->client->voiceFloodPenalty)
        {
            if (ent->client->voiceFloodPenalty > level.time)
            {
                return;
            }

            // No longer penalized
            ent->client->voiceFloodPenalty = 0;
        }

        // See if this client flooded with voice chats
        ent->client->voiceFloodCount++;
        if (ent->client->voiceFloodCount >= g_voiceFloodCount.integer)
        {
            ent->client->voiceFloodCount = 0;
            ent->client->voiceFloodTimer = 0;
            ent->client->voiceFloodPenalty = level.time + g_voiceFloodPenalty.integer * 1000;

            trap_SendServerCommand(ent - g_entities, va("print \"Voice chat flooded, you will be able use voice chats again in (%d) seconds\n\"", g_voiceFloodPenalty.integer));

            return;
        }
    }

    G_GetChatPrefix(ent, target, mode, name, sizeof(name));

    if (target)
    {
        G_VoiceTo(ent, target, mode, name, id, voiceonly);
        return;
    }

    // send it to all the apropriate clients
    for (j = 0; j < level.maxclients; j++)
    {
        other = &g_entities[j];
        G_VoiceTo(ent, other, mode, name, id, voiceonly);
    }
}

/*
==================
Cmd_Voice_f
==================
*/
static void Cmd_Voice_f(gentity_t *ent, int mode, qboolean arg0, qboolean voiceonly)
{
    char *p;

    if (trap_Argc() < 2 && !arg0)
    {
        return;
    }

    if (arg0)
    {
        p = ConcatArgs(0);
    }
    else
    {
        p = ConcatArgs(1);
    }

    // OSP
    if (strlen(p) >= 512)
    {
        return;
    }

    ent->client->ospStats->numChats++;
    // END

    G_Voice(ent, NULL, mode, p, voiceonly);
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f(gentity_t *ent)
{
    // OSP
    //trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", vtos(ent->s.origin)));
    trap_SendServerCommand(ent - g_entities, va("print \"%s\n\"", vtos(ent->r.currentOrigin)));
    // END
}

/*
============
G_VoteDisabled

determins if the given vote is disabled
============
*/
int G_VoteDisabled(const char *callvote)
{
    return trap_Cvar_VariableIntegerValue(va("novote_%s", callvote));
}

/*
==================
Cmd_CallVote_f
==================
*/
void Cmd_CallVote_f(gentity_t *ent)
{
    int i;
    char arg1[MAX_STRING_TOKENS];
    char arg2[MAX_STRING_TOKENS];

    if (!g_allowVote.integer)
    {
        trap_SendServerCommand(ent - g_entities, "print \"Voting not allowed here.\n\"");
        return;
    }

    if (level.intermissiontime || level.intermissionQueued)
    {
        trap_SendServerCommand(ent - g_entities, "print \"Voting not allowed during intermission.\n\"");
        return;
    }

    // No voting within the minute of a map change
    if (level.time - level.startTime < 1000 * 60)
    {
        trap_SendServerCommand(ent - g_entities, "print \"Cannot vote within the first minute of a map change.\n\"");
        return;
    }

    if (level.numConnectedClients > 1 && level.numVotingClients == 1)
    {
        trap_SendServerCommand(ent - g_entities, "print \"You need at least 2 clients to call a vote.\n\"");
        return;
    }

    if (level.voteTime)
    {
        trap_SendServerCommand(ent - g_entities, "print \"A vote is already in progress.\n\"");
        return;
    }

    if (ent->client->pers.voteCount >= MAX_VOTE_COUNT)
    {
        trap_SendServerCommand(ent - g_entities, "print \"You have called the maximum number of votes.\n\"");
        return;
    }

    if (ent->client->sess.team == TEAM_SPECTATOR)
    {
        trap_SendServerCommand(ent - g_entities, "print \"Not allowed to call a vote as spectator.\n\"");
        return;
    }

    if (ent->client->voteDelayTime > level.time)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"You are not allowed to vote within %d minute of a failed vote.\n\"", g_failedVoteDelay.integer));
        return;
    }

    // Save the voting client id
    level.voteClient = ent->s.number;

    // make sure it is a valid command to vote on
    trap_Argv(1, arg1, sizeof(arg1));
    trap_Argv(2, arg2, sizeof(arg2));

    if (strchr(arg1, ';') || strchr(arg2, ';'))
    {
        trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
        return;
    }

    if (!Q_stricmp(arg1, "map_restart"))
    {
    }
    else if (!Q_stricmp(arg1, "mapcycle"))
    {
    }
    else if (!Q_stricmp(arg1, "map"))
    {
    }
    else if (!Q_stricmp(arg1, "rmgmap"))
    {
    }
    else if (!Q_stricmp(arg1, "g_gametype"))
    {
    }
    else if (!Q_stricmp(arg1, "kick"))
    {
    }
    else if (!Q_stricmp(arg1, "clientkick"))
    {
    }
    else if (!Q_stricmp(arg1, "g_doWarmup"))
    {
    }
    else if (!Q_stricmp(arg1, "g_friendlyfire"))
    {
    }
    else if (!Q_stricmp(arg1, "timelimit"))
    {
    }
    else if (!Q_stricmp(arg1, "timeextension"))
    {
    }
    else if (!Q_stricmp(arg1, "scorelimit"))
    {
    }
    else
    {
        trap_SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
        trap_SendServerCommand(ent - g_entities, "print \"Vote commands are: map_restart, nextmap, map <mapname>, g_gametype <n>, kick <player>, clientkick <clientnum>, g_doWarmup, timelimit <time>, scorelimit <score>.\n\"");
        return;
    }

    // see if this particular vote is disabled
    if (G_VoteDisabled(arg1))
    {
        trap_SendServerCommand(ent - g_entities, va("print \"The '%s' vote has been disabled on this server.\n\"", arg1));
        return;
    }

    // if there is still a vote to be executed
    if (level.voteExecuteTime)
    {
        level.voteExecuteTime = 0;
        trap_SendConsoleCommand(EXEC_APPEND, va("%s\n", level.voteString));
    }

    // special case for g_gametype, check for bad values
    if (!Q_stricmp(arg1, "g_gametype"))
    {
        // Verify the gametype
        i = BG_FindGametype(arg2);
        if (i < 0)
        {
            trap_SendServerCommand(ent - g_entities, "print \"Invalid gametype.\n\"");
            return;
        }

        Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s %s", arg1, bg_gametypeData[i].name);
    }
    else if (!Q_stricmp(arg1, "map"))
    {
        if (!G_DoesMapExist(arg2))
        {
            trap_SendServerCommand(ent - g_entities, "print \"Unknown mapname.\n\"");
            return;
        }

        Com_sprintf(level.voteString, sizeof(level.voteString), "%s %s", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
    }
    else if (!Q_stricmp(arg1, "rmgmap"))
    {
        char arg3[MAX_STRING_TOKENS];
        char arg4[MAX_STRING_TOKENS];

        trap_Argv(3, arg3, sizeof(arg3));
        trap_Argv(4, arg4, sizeof(arg4));

        Com_sprintf(level.voteString, sizeof(level.voteString), "rmgmap 1 %s 2 %s 3 %s 4 \"%s\" 0", arg2, arg3, arg4, ConcatArgs(5));
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "Randomly Generated Map");
    }
    else if (!Q_stricmp(arg1, "mapcycle"))
    {
        if (!*g_mapcycle.string || !Q_stricmp(g_mapcycle.string, "none"))
        {
            trap_SendServerCommand(ent - g_entities, "print \"there is no map cycle currently set up.\n\"");
            return;
        }

        Com_sprintf(level.voteString, sizeof(level.voteString), "mapcycle");
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "nextmap");
    }
    else if (!Q_stricmp(arg1, "clientkick"))
    {
        int n = atoi(arg2);

        if (n < 0 || n >= MAX_CLIENTS)
        {
            trap_SendServerCommand(ent - g_entities, va("print \"invalid client number %d.\n\"", n));
            return;
        }

        if (g_entities[n].client->pers.connected == CON_DISCONNECTED)
        {
            trap_SendServerCommand(ent - g_entities, va("print \"there is no client with the client number %d.\n\"", n));
            return;
        }

        if (g_voteKickBanTime.integer)
        {
            Com_sprintf(level.voteString, sizeof(level.voteString), "banclient %s %d voted off server", arg2, g_voteKickBanTime.integer);
        }
        else
        {
            Com_sprintf(level.voteString, sizeof(level.voteString), "clientkick %s", arg2);
        }

        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", g_entities[n].client->pers.netname);
    }
    else if (!Q_stricmp(arg1, "kick"))
    {
        int clientid = G_ClientNumberFromName(arg2);

        if (clientid == -1)
        {
            trap_SendServerCommand(ent - g_entities, va("print \"there is no client named '%s' currently on the server.\n\"", arg2));
            return;
        }

        if (g_voteKickBanTime.integer)
        {
            Com_sprintf(level.voteString, sizeof(level.voteString), "banclient %d %d voted off server", clientid, g_voteKickBanTime.integer);
        }
        else
        {
            Com_sprintf(level.voteString, sizeof(level.voteString), "clientkick %d", clientid);
        }

        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "kick %s", g_entities[clientid].client->pers.netname);
    }
    else if (!Q_stricmp(arg1, "timeextension"))
    {
        if (!g_timelimit.integer)
        {
            trap_SendServerCommand(ent - g_entities, va("print \"There is no timelimit to extend.\n\""));
            return;
        }

        if (!g_timeextension.integer)
        {
            trap_SendServerCommand(ent - g_entities, va("print \"This server does not allow time extensions.\n\""));
            return;
        }

        Com_sprintf(level.voteString, sizeof(level.voteString), "extendtime %d", g_timeextension.integer);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "extend timelimit by %d minutes", g_timeextension.integer);
    }
    else if (!Q_stricmp(arg1, "timelimit") ||
             !Q_stricmp(arg1, "scorelimit") ||
             !Q_stricmp(arg1, "map_restart") ||
             !Q_stricmp(arg1, "g_doWarmup") ||
             !Q_stricmp(arg1, "g_friendlyfire"))
    {
        Com_sprintf(level.voteString, sizeof(level.voteString), "%s %d", arg1, atoi(arg2));
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
    }
    else
    {
        Com_sprintf(level.voteString, sizeof(level.voteString), "%s \"%s\"", arg1, arg2);
        Com_sprintf(level.voteDisplayString, sizeof(level.voteDisplayString), "%s", level.voteString);
    }

    trap_SendServerCommand(-1, va("print \"%s has called a vote.\n\"", ent->client->pers.netname));

    // start the voting, the caller autoamtically votes yes
    level.voteTime = level.time;
    level.voteYes = 1;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++)
    {
        level.clients[i].ps.eFlags &= ~EF_VOTED;
    }
    ent->client->ps.eFlags |= EF_VOTED;

    trap_SetConfigstring(CS_VOTE_TIME, va("%i,%i", level.voteTime, g_voteDuration.integer * 1000));
    trap_SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
    trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
    trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
    trap_SetConfigstring(CS_VOTE_NEEDED, va("%i", level.numVotingClients / 2));
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f(gentity_t *ent)
{
    char msg[64];

    if (!level.voteTime)
    {
        trap_SendServerCommand(ent - g_entities, "print \"No vote in progress.\n\"");
        return;
    }

    if (ent->client->ps.eFlags & EF_VOTED)
    {
        trap_SendServerCommand(ent - g_entities, "print \"Vote already cast.\n\"");
        return;
    }

    if (ent->client->sess.team == TEAM_SPECTATOR)
    {
        // OSP
        //trap_SendServerCommand(ent - g_entities, "print \"Not allowed to vote as spectator.\n\"");
        trap_SendServerCommand(ent - g_entities, "print \"Not allowed to vote as a spectator.\n\"");
        // END
        return;
    }

    trap_SendServerCommand(ent - g_entities, "print \"Vote cast.\n\"");

    ent->client->ps.eFlags |= EF_VOTED;

    trap_Argv(1, msg, sizeof(msg));

    if (msg[0] == 'y' || msg[1] == 'Y' || msg[1] == '1')
    {
        level.voteYes++;
        trap_SetConfigstring(CS_VOTE_YES, va("%i", level.voteYes));
    }
    else
    {
        level.voteNo++;
        trap_SetConfigstring(CS_VOTE_NO, va("%i", level.voteNo));
    }

    // a majority will be determined in CheckVote, which will also account
    // for players entering or leaving
}

/*
=================
Cmd_Ignore_f
=================
*/
void Cmd_Ignore_f(gentity_t *ent)
{
    char buffer[MAX_TOKEN_CHARS];
    int ignoree;
    qboolean ignore;

    trap_Argv(1, buffer, sizeof(buffer));

    ignoree = atoi(buffer);

    // OSP
    // CVE-2005-2115
    if (ignoree < 0 || ignoree > 64)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"Invalid Client Num.\n\""));
        return;
    }
    // END

    ignore = G_IsClientChatIgnored(ent->s.number, ignoree) ? qfalse : qtrue;

    if (ignoree == ent->s.number)
    {
        // OSP
        //trap_SendServerCommand(ent - g_entities, va("print \"cant ignore yourself.\n\""));
        trap_SendServerCommand(ent - g_entities, va("print \"can't ignore yourself.\n\""));
        // END
        return;
    }

    G_IgnoreClientChat(ent->s.number, ignoree, ignore);

    if (ignore)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"%s ignored.\n\"", g_entities[ignoree].client->pers.netname));
    }
    else
    {
        trap_SendServerCommand(ent - g_entities, va("print \"%s unignored.\n\"", g_entities[ignoree].client->pers.netname));
    }
}

/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f(gentity_t *ent)
{
    vec3_t origin, angles;
    char buffer[MAX_TOKEN_CHARS];
    int i;

    if (!g_cheats.integer)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"Cheats are not enabled on this server.\n\""));
        return;
    }

    if (trap_Argc() != 5)
    {
        trap_SendServerCommand(ent - g_entities, va("print \"usage: setviewpos x y z yaw\n\""));
        return;
    }

    VectorClear(angles);
    for (i = 0; i < 3; i++)
    {
        trap_Argv(i + 1, buffer, sizeof(buffer));
        origin[i] = atof(buffer);
    }

    trap_Argv(4, buffer, sizeof(buffer));
    angles[YAW] = atof(buffer);

    TeleportPlayer(ent, origin, angles);
}

/*
=================
ClientCommand
=================
*/
void ClientCommand(int clientNum)
{
    gentity_t *ent;
    char cmd[MAX_TOKEN_CHARS];

    ent = g_entities + clientNum;
    if (!ent->client)
    {
        return; // not fully in game yet
    }

    trap_Argv(0, cmd, sizeof(cmd));

    //rww - redirect bot commands
    if (strstr(cmd, "bot_") && AcceptBotCommand(cmd, ent))
    {
        return;
    }
    //end rww

    if (Q_stricmp(cmd, "say") == 0)
    {
        Cmd_Say_f(ent, SAY_ALL, qfalse);
        return;
    }
    if (Q_stricmp(cmd, "say_team") == 0)
    {
        Cmd_Say_f(ent, SAY_TEAM, qfalse);
        return;
    }

    if (Q_stricmp(cmd, "tell") == 0)
    {
        Cmd_Tell_f(ent);
        return;
    }

    if (Q_stricmp(cmd, "vsay_team") == 0)
    {
        Cmd_Voice_f(ent, SAY_TEAM, qfalse, qfalse);
        return;
    }

    if (Q_stricmp(cmd, "score") == 0)
    {
        Cmd_Score_f(ent);
        return;
    }

    if (Q_stricmp(cmd, "team") == 0)
    {
        Cmd_Team_f(ent);
        return;
    }

    // OSP
    if (OSP_ClientCommand(ent, cmd, qtrue))
    {
        return;
    }
    // END

    // ignore all other commands when at intermission
    if (level.intermissiontime)
    {
        //		Cmd_Say_f (ent, qfalse, qtrue);
        return;
    }

    if (Q_stricmp(cmd, "drop") == 0)
        Cmd_Drop_f(ent);
    else if (Q_stricmp(cmd, "dropitem") == 0)
        Cmd_DropItem_f(ent);
    else if (Q_stricmp(cmd, "give") == 0)
        Cmd_Give_f(ent);
    else if (Q_stricmp(cmd, "god") == 0)
        Cmd_God_f(ent);
    else if (Q_stricmp(cmd, "notarget") == 0)
        Cmd_Notarget_f(ent);
    else if (Q_stricmp(cmd, "noclip") == 0)
        Cmd_Noclip_f(ent);
    else if (Q_stricmp(cmd, "kill") == 0)
        Cmd_Kill_f(ent);
    else if (Q_stricmp(cmd, "levelshot") == 0)
        Cmd_LevelShot_f(ent);
    else if (Q_stricmp(cmd, "follow") == 0)
        Cmd_Follow_f(ent);
    else if (Q_stricmp(cmd, "follownext") == 0)
        Cmd_FollowCycle_f(ent, 1);
    else if (Q_stricmp(cmd, "followprev") == 0)
        Cmd_FollowCycle_f(ent, -1);
    else if (Q_stricmp(cmd, "where") == 0)
        Cmd_Where_f(ent);
    else if (Q_stricmp(cmd, "callvote") == 0)
        Cmd_CallVote_f(ent);
    else if (Q_stricmp(cmd, "vote") == 0)
        Cmd_Vote_f(ent);
    else if (Q_stricmp(cmd, "setviewpos") == 0)
        Cmd_SetViewpos_f(ent);
    else if (Q_stricmp(cmd, "ignore") == 0)
        Cmd_Ignore_f(ent);

#ifdef _SOF2_BOTS
    else if (Q_stricmp(cmd, "addbot") == 0)
        trap_SendServerCommand(clientNum, va("print \"ADDBOT command can only be used via RCON\n\""));
#endif

    // OSP
    else if (OSP_ClientCommand(ent, cmd, qfalse))
        return;
    // END

    else
        trap_SendServerCommand(clientNum, va("print \"unknown cmd %s\n\"", cmd));
}
