// Copyright (C) 2001-2002 Raven Software.
//

#include "gt_local.h"

void GT_Init(void);
void GT_RunFrame(int time);
int GT_Event(int cmd, int time, int arg0, int arg1, int arg2, int arg3, int arg4);

gametypeLocals_t gametype;

typedef struct
{
    vmCvar_t *vmCvar;
    char *cvarName;
    char *defaultString;
    int cvarFlags;
    float mMinValue, mMaxValue;
    int modificationCount; // for tracking changes
    qboolean trackChange;  // track this variable, and announce if changed
    qboolean teamShader;   // track and if changed, update shader state
} cvarTable_t;

vmCvar_t gt_flagReturnTime;

static cvarTable_t gametypeCvarTable[] = {
    {NULL, NULL, NULL, 0, 0.0f, 0.0f, 0, qfalse},
};

/*
================
vmMain

This is the only way control passes into the module.
This must be the very first function compiled into the .q3vm file
================
*/
int vmMain(int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11)
{
    switch (command)
    {
    case GAMETYPE_INIT:
        GT_Init();
        return 0;

    case GAMETYPE_START:
        return 0;

    case GAMETYPE_RUN_FRAME:
        GT_RunFrame(arg0);
        return 0;

    case GAMETYPE_EVENT:
        return GT_Event(arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    return -1;
}

/*
=================
GT_RegisterCvars
=================
*/
void GT_RegisterCvars(void)
{
    cvarTable_t *cv;

    for (cv = gametypeCvarTable; cv->cvarName != NULL; cv++)
    {
        trap_Cvar_Register(cv->vmCvar, cv->cvarName, cv->defaultString, cv->cvarFlags, cv->mMinValue, cv->mMaxValue);

        if (cv->vmCvar)
        {
            cv->modificationCount = cv->vmCvar->modificationCount;
        }
    }
}

/*
=================
GT_UpdateCvars
=================
*/
void GT_UpdateCvars(void)
{
    cvarTable_t *cv;

    for (cv = gametypeCvarTable; cv->cvarName != NULL; cv++)
    {
        if (cv->vmCvar)
        {
            trap_Cvar_Update(cv->vmCvar);

            if (cv->modificationCount != cv->vmCvar->modificationCount)
            {
                cv->modificationCount = cv->vmCvar->modificationCount;
            }
        }
    }
}

/*
================
GT_Init

initializes the gametype by spawning the gametype items and 
preparing them
================
*/
void GT_Init(void)
{
    memset(&gametype, 0, sizeof(gametype));

    // Register all cvars for this gametype
    GT_RegisterCvars();
}

// OSP
char *OSP_GetTeamName(int team)
{
    switch (team)
    {
    case TEAM_FREE:
        return "FFA";
    case TEAM_RED:
    {
        char name[64];
        memset(name, 0, sizeof(name));
        trap_Cvar_VariableStringBuffer("team_redName", name, sizeof(name) - 1);
        return va("%s", name);
    }
    case TEAM_BLUE:
    {
        char name[64];
        memset(name, 0, sizeof(name));
        trap_Cvar_VariableStringBuffer("team_blueName", name, sizeof(name) - 1);
        return va("%s", name);
    }
    case TEAM_SPECTATOR:
        return "Spectators";
    }

    return "unknown team";
}
// END

/*
================
GT_RunFrame

Runs all thinking code for gametype
================
*/
void GT_RunFrame(int time)
{
    gametype.time = time;

    GT_UpdateCvars();
}

/*
================
GT_Event

Handles all events sent to the gametype
================
*/
int GT_Event(int cmd, int time, int arg0, int arg1, int arg2, int arg3, int arg4)
{
    switch (cmd)
    {
    case GTEV_TEAM_ELIMINATED:
        switch (arg0)
        {
        case TEAM_RED:
            // OSP
            //trap_Cmd_TextMessage(-1, "Red team eliminated!");
            trap_Cmd_TextMessage(-1, va("%s eliminated!", OSP_GetTeamName(TEAM_RED)));
            // END
            trap_Cmd_AddTeamScore(TEAM_BLUE, 1);
            trap_Cmd_Restart(5);
            break;

        case TEAM_BLUE:
            // OSP
            //trap_Cmd_TextMessage(-1, "Blue team eliminated!");
            trap_Cmd_TextMessage(-1, va("%s eliminated!", OSP_GetTeamName(TEAM_BLUE)));
            // END
            trap_Cmd_AddTeamScore(TEAM_RED, 1);
            trap_Cmd_Restart(5);
            break;
        }
        break;

    case GTEV_TIME_EXPIRED:
        trap_Cmd_TextMessage(-1, "Round Draw!");
        trap_Cmd_Restart(5);
        break;
    }

    return 0;
}

#ifndef GAMETYPE_HARD_LINKED
// this is only here so the functions in q_shared.c and bg_*.c can link (FIXME)

void QDECL Com_Error(int level, const char *msg, ...)
{
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    vsprintf(text, msg, argptr);
    va_end(argptr);

    trap_Error(text);
}

void QDECL Com_Printf(const char *msg, ...)
{
    va_list argptr;
    char text[1024];

    va_start(argptr, msg);
    vsprintf(text, msg, argptr);
    va_end(argptr);

    trap_Print(text);
}

#endif
