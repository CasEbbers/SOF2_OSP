// Copyright (C) 2001-2002 Raven Software.
//
// cg_view.c -- setup all the parameters (position, angle, etc)
// for a 3D rendering
#include "cg_local.h"

#if !defined(CL_LIGHT_H_INC)
#include "cg_lights.h"
#endif

/*
=============================================================================

  MODEL TESTING

The viewthing and gun positioning tools from Q2 have been integrated and
enhanced into a single model testing facility.

Model viewing can begin with either "testmodel <modelname>" or "testgun <modelname>".

The names must be the full pathname after the basedir, like 
"models/weapons/v_launch/tris.md3" or "players/male/tris.md3"

Testmodel will create a fake entity 100 units in front of the current view
position, directly facing the viewer.  It will remain immobile, so you can
move around it to view it from different angles.

Testgun will cause the model to follow the player around and supress the real
view weapon model.  The default frame 0 of most guns is completely off screen,
so you will probably have to cycle a couple frames to see it.

"nextframe", "prevframe", "nextskin", and "prevskin" commands will change the
frame or skin of the testmodel.  These are bound to F5, F6, F7, and F8 in
q3default.cfg.

If a gun is being tested, the "gun_x", "gun_y", and "gun_z" variables will let
you adjust the positioning.

Note that none of the model testing features update while the game is paused, so
it may be convenient to test with deathmatch set to 1 so that bringing down the
console doesn't pause the game.

=============================================================================
*/

/*
=================
CG_TestModel_f

Creates an entity in front of the current position, which
can then be moved around
=================
*/
void CG_TestModel_f(void)
{
    vec3_t angles;

    memset(&cg.testModelEntity, 0, sizeof(cg.testModelEntity));
    if (trap_Argc() < 2)
    {
        return;
    }

    Q_strncpyz(cg.testModelName, CG_Argv(1), MAX_QPATH);
    cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);

    if (trap_Argc() == 3)
    {
        cg.testModelEntity.backlerp = atof(CG_Argv(2));
        cg.testModelEntity.frame = 1;
        cg.testModelEntity.oldframe = 0;
    }

    if (!cg.testModelEntity.hModel)
    {
        Com_Printf("Can't register model\n");
        return;
    }

    VectorMA(cg.refdef.vieworg, 100, cg.refdef.viewaxis[0], cg.testModelEntity.origin);

    angles[PITCH] = 0;
    angles[YAW] = 180 + cg.refdef.viewangles[1];
    angles[ROLL] = 0;

    AnglesToAxis(angles, cg.testModelEntity.axis);
}

void CG_TestModelNextFrame_f(void)
{
    cg.testModelEntity.frame++;
    Com_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelPrevFrame_f(void)
{
    cg.testModelEntity.frame--;
    if (cg.testModelEntity.frame < 0)
    {
        cg.testModelEntity.frame = 0;
    }

    Com_Printf("frame %i\n", cg.testModelEntity.frame);
}

void CG_TestModelNextSkin_f(void)
{
    cg.testModelEntity.skinNum++;
    Com_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

void CG_TestModelPrevSkin_f(void)
{
    cg.testModelEntity.skinNum--;
    if (cg.testModelEntity.skinNum < 0)
    {
        cg.testModelEntity.skinNum = 0;
    }
    Com_Printf("skin %i\n", cg.testModelEntity.skinNum);
}

static void CG_AddTestModel(void)
{
    // re-register the model, because the level may have changed
    cg.testModelEntity.hModel = trap_R_RegisterModel(cg.testModelName);
    if (!cg.testModelEntity.hModel)
    {
        Com_Printf("Can't register model\n");
        return;
    }

    // OSP
    //trap_R_AddRefEntityToScene(&cg.testModelEntity);
    sub_0002794a(&cg.testModelEntity);
    // END
}

/*
=================
CG_CalcVrect

Sets the coordinates of the rendered window
=================
*/
static void CG_CalcVrect(void)
{
    int size;

    size = 100;

    cg.refdef.width = cgs.glconfig.vidWidth * size / 100;
    cg.refdef.width &= ~1;

    cg.refdef.height = cgs.glconfig.vidHeight * size / 100;
    cg.refdef.height &= ~1;

    cg.refdef.x = (cgs.glconfig.vidWidth - cg.refdef.width) / 2;
    cg.refdef.y = (cgs.glconfig.vidHeight - cg.refdef.height) / 2;
}

//==============================================================================

//==============================================================================
//==============================================================================
// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset(void)
{
    int timeDelta;

    // smooth out stair climbing
    timeDelta = cg.time - cg.stepTime;
    if (timeDelta < STEP_TIME)
    {
        cg.refdef.vieworg[2] -= cg.stepChange * (STEP_TIME - timeDelta) / STEP_TIME;
    }
}

#define CAMERA_DAMP_INTERVAL 50

static vec3_t cameramins = {-4, -4, -4};
static vec3_t cameramaxs = {4, 4, 4};
vec3_t camerafwd, cameraup;

vec3_t cameraFocusAngles, cameraFocusLoc;
vec3_t cameraIdealTarget, cameraIdealLoc;
vec3_t cameraCurTarget = {0, 0, 0}, cameraCurLoc = {0, 0, 0};
vec3_t cameraOldLoc = {0, 0, 0}, cameraNewLoc = {0, 0, 0};
int cameraLastFrame = 0;

/*
===============
Notes on the camera viewpoint in and out...

cg.refdef.vieworg
--at the start of the function holds the player actor's origin (center of player model).
--it is set to the final view location of the camera at the end of the camera code.
cg.refdef.viewangles
--at the start holds the client's view angles
--it is set to the final view angle of the camera at the end of the camera code.

===============
*/

/*
===============
CG_CalcTargetThirdPersonViewLocation

===============
*/
static void CG_CalcIdealThirdPersonViewTarget(void)
{
    // Initialize IdealTarget
    VectorCopy(cg.refdef.vieworg, cameraFocusLoc);

    // Add in the new viewheight
    cameraFocusLoc[2] += cg.snap->ps.viewheight;

    // Add in a vertical offset from the viewpoint, which puts the actual target above the head, regardless of angle.
    VectorMA(cameraFocusLoc, 1, cameraup, cameraIdealTarget);
}

/*
===============
CG_CalcTargetThirdPersonViewLocation

===============
*/
static void CG_CalcIdealThirdPersonViewLocation(void)
{
    VectorMA(cameraIdealTarget, -(cg_thirdPersonRange.value), camerafwd, cameraIdealLoc);
}

static void CG_ResetThirdPersonViewDamp(void)
{
    trace_t trace;

    // Cap the pitch within reasonable limits
    if (cameraFocusAngles[PITCH] > 89.0)
    {
        cameraFocusAngles[PITCH] = 89.0;
    }
    else if (cameraFocusAngles[PITCH] < -89.0)
    {
        cameraFocusAngles[PITCH] = -89.0;
    }

    AngleVectors(cameraFocusAngles, camerafwd, NULL, cameraup);

    // Set the cameraIdealTarget
    CG_CalcIdealThirdPersonViewTarget();

    // Set the cameraIdealLoc
    CG_CalcIdealThirdPersonViewLocation();

    // Now, we just set everything to the new positions.
    VectorCopy(cameraIdealLoc, cameraCurLoc);
    VectorCopy(cameraIdealTarget, cameraCurTarget);

    // First thing we do is trace from the first person viewpoint out to the new target location.
    CG_Trace(&trace, cameraFocusLoc, cameramins, cameramaxs, cameraCurTarget, cg.snap->ps.clientNum, MASK_SOLID | CONTENTS_PLAYERCLIP);
    if (trace.fraction <= 1.0)
    {
        VectorCopy(trace.endpos, cameraCurTarget);
    }

    // Now we trace from the new target location to the new view location, to make sure there is nothing in the way.
    CG_Trace(&trace, cameraCurTarget, cameramins, cameramaxs, cameraCurLoc, cg.snap->ps.clientNum, MASK_SOLID | CONTENTS_PLAYERCLIP);
    if (trace.fraction <= 1.0)
    {
        VectorCopy(trace.endpos, cameraCurLoc);
    }

    cameraLastFrame = cg.time;
}

// This is called every frame.
static void CG_UpdateThirdPersonTargetDamp(void)
{
    trace_t trace;
    vec3_t targetdiff;
    float dampfactor, dtime, ratio;
    float damp = 1.0f;

    // Set the cameraIdealTarget
    // Automatically get the ideal target, to avoid jittering.
    CG_CalcIdealThirdPersonViewTarget();

    if (damp >= 1.0)
    { // No damping.
        VectorCopy(cameraIdealTarget, cameraCurTarget);
    }
    else if (damp >= 0.0)
    {
        // Calculate the difference from the current position to the new one.
        VectorSubtract(cameraIdealTarget, cameraCurTarget, targetdiff);

        // Now we calculate how much of the difference we cover in the time allotted.
        // The equation is (Damp)^(time)
        dampfactor = 1.0 - damp;                                                          // We must exponent the amount LEFT rather than the amount bled off
        dtime = (float)(cg.time - cameraLastFrame) * (1.0 / (float)CAMERA_DAMP_INTERVAL); // Our dampfactor is geared towards a time interval equal to "1".

        // Note that since there are a finite number of "practical" delta millisecond values possible,
        // the ratio should be initialized into a chart ultimately.
        ratio = powf(dampfactor, (int)dtime);

        // This value is how much distance is "left" from the ideal.
        VectorMA(cameraIdealTarget, -ratio, targetdiff, cameraCurTarget);
        /////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // Now we trace to see if the new location is cool or not.

    // First thing we do is trace from the first person viewpoint out to the new target location.
    CG_Trace(&trace, cameraFocusLoc, cameramins, cameramaxs, cameraCurTarget, cg.snap->ps.clientNum, MASK_SOLID | CONTENTS_PLAYERCLIP);
    if (trace.fraction < 1.0)
    {
        VectorCopy(trace.endpos, cameraCurTarget);
    }

    // Note that previously there was an upper limit to the number of physics traces that are done through the world
    // for the sake of camera collision, since it wasn't calced per frame.  Now it is calculated every frame.
    // This has the benefit that the camera is a lot smoother now (before it lerped between tested points),
    // however two full volume traces each frame is a bit scary to think about.
}

// This can be called every interval, at the user's discretion.
static void CG_UpdateThirdPersonCameraDamp(void)
{
    trace_t trace;
    vec3_t locdiff;
    float dampfactor, dtime, ratio;
    float damp = 1.0f;

    // Set the cameraIdealLoc
    CG_CalcIdealThirdPersonViewLocation();

    // First thing we do is calculate the appropriate damping factor for the camera.
    dampfactor = 0.0;
    if (damp != 0.0)
    {
        double pitch;

        // Note that the camera pitch has already been capped off to 89.
        pitch = Q_fabs(cameraFocusAngles[PITCH]);

        // The higher the pitch, the larger the factor, so as you look up, it damps a lot less.
        pitch /= 89.0;
        dampfactor = (1.0 - damp) * (pitch * pitch);

        dampfactor += damp;
    }

    if (dampfactor >= 1.0)
    { // No damping.
        VectorCopy(cameraIdealLoc, cameraCurLoc);
    }
    else if (dampfactor >= 0.0)
    {
        // Calculate the difference from the current position to the new one.
        VectorSubtract(cameraIdealLoc, cameraCurLoc, locdiff);

        // Now we calculate how much of the difference we cover in the time allotted.
        // The equation is (Damp)^(time)
        dampfactor = 1.0 - dampfactor;                                                    // We must exponent the amount LEFT rather than the amount bled off
        dtime = (float)(cg.time - cameraLastFrame) * (1.0 / (float)CAMERA_DAMP_INTERVAL); // Our dampfactor is geared towards a time interval equal to "1".

        // Note that since there are a finite number of "practical" delta millisecond values possible,
        // the ratio should be initialized into a chart ultimately.
        ratio = powf(dampfactor, (int)dtime);

        // This value is how much distance is "left" from the ideal.
        VectorMA(cameraIdealLoc, -ratio, locdiff, cameraCurLoc);
        /////////////////////////////////////////////////////////////////////////////////////////////////////////
    }

    // Now we trace from the new target location to the new view location, to make sure there is nothing in the way.
    CG_Trace(&trace, cameraCurTarget, cameramins, cameramaxs, cameraCurLoc, cg.snap->ps.clientNum, MASK_SOLID | CONTENTS_PLAYERCLIP);
    if (trace.fraction < 1.0)
    {
        VectorCopy(trace.endpos, cameraCurLoc);
        //FIXME: when the trace hits movers, it gets very very jaggy... ?
        /*
        //this doesn't actually help any
        if ( trace.entityNum != ENTITYNUM_WORLD )
        {
            centity_t *cent = &cg_entities[trace.entityNum];
            gentity_t *gent = &g_entities[trace.entityNum];
            if ( cent != NULL && gent != NULL )
            {
                if ( cent->currentState.pos.trType == TR_LINEAR || cent->currentState.pos.trType == TR_LINEAR_STOP )
                {
                    vec3_t	diff;
                    VectorSubtract( cent->lerpOrigin, gent->currentOrigin, diff );
                    VectorAdd( cameraCurLoc, diff, cameraCurLoc );
                }
            }
        }
        */
    }

    // Note that previously there was an upper limit to the number of physics traces that are done through the world
    // for the sake of camera collision, since it wasn't calced per frame.  Now it is calculated every frame.
    // This has the benefit that the camera is a lot smoother now (before it lerped between tested points),
    // however two full volume traces each frame is a bit scary to think about.
}

/*
===============`
CG_OffsetThirdPersonView

===============
*/
extern vmCvar_t cg_thirdPersonHorzOffset;
static void CG_OffsetThirdPersonView(void)
{
    vec3_t diff;

    // Set camera viewing direction.
    VectorCopy(cg.refdef.viewangles, cameraFocusAngles);

    // if dead, look at killer
    if (0 && cg.snap->ps.stats[STAT_HEALTH] <= 0)
    {
        cameraFocusAngles[YAW] = cg.snap->ps.stats[STAT_DEAD_YAW];
        cameraFocusAngles[PITCH] = 90;
    }
    else
    { // Add in the third Person Angle.
        cameraFocusAngles[YAW] += cg_thirdPersonYaw.value;
        cameraFocusAngles[PITCH] += cg_thirdPersonPitch.value;
    }

    // The next thing to do is to see if we need to calculate a new camera target location.

    // If we went back in time for some reason, or if we just started, reset the sample.
    if (cameraLastFrame == 0 || cameraLastFrame > cg.time)
    {
        CG_ResetThirdPersonViewDamp();
    }
    else
    {
        // Cap the pitch within reasonable limits
        if (cameraFocusAngles[PITCH] > 89.0)
        {
            cameraFocusAngles[PITCH] = 89.0;
        }
        else if (cameraFocusAngles[PITCH] < -89.0)
        {
            cameraFocusAngles[PITCH] = -89.0;
        }

        AngleVectors(cameraFocusAngles, camerafwd, NULL, cameraup);

        // Move the target to the new location.
        CG_UpdateThirdPersonTargetDamp();
        CG_UpdateThirdPersonCameraDamp();
    }

    // Now interestingly, the Quake method is to calculate a target focus point above the player, and point the camera at it.
    // We won't do that for now.

    // We must now take the angle taken from the camera target and location.
    VectorSubtract(cameraCurTarget, cameraCurLoc, diff);
    VectorNormalize(diff);
    vectoangles(diff, cg.refdef.viewangles);

    // Temp: just move the camera to the side a bit
    if (cg_thirdPersonHorzOffset.value != 0.0f)
    {
        AnglesToAxis(cg.refdef.viewangles, cg.refdef.viewaxis);
        VectorMA(cameraCurLoc, cg_thirdPersonHorzOffset.value, cg.refdef.viewaxis[1], cameraCurLoc);
    }

    // ...and of course we should copy the new view location to the proper spot too.
    VectorCopy(cameraCurLoc, cg.refdef.vieworg);

    cameraLastFrame = cg.time;
}

/*
===============
CG_OffsetThirdPersonView

===============
*/
/*
#define	FOCUS_DISTANCE	512
static void CG_OffsetThirdPersonView( void ) {
    vec3_t		forward, right, up;
    vec3_t		view;
    vec3_t		focusAngles;
    trace_t		trace;
    static vec3_t	mins = { -4, -4, -4 };
    static vec3_t	maxs = { 4, 4, 4 };
    vec3_t		focusPoint;
    float		focusDist;
    float		forwardScale, sideScale;

    cg.refdef.vieworg[2] += cg.predictedPlayerState.viewheight;

    VectorCopy( cg.refdef.viewangles, focusAngles );

    // if dead, look at killer
    if ( cg.predictedPlayerState.stats[STAT_HEALTH] <= 0 ) {
        focusAngles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
        cg.refdef.viewangles[YAW] = cg.predictedPlayerState.stats[STAT_DEAD_YAW];
    }

    if ( focusAngles[PITCH] > 45 ) {
        focusAngles[PITCH] = 45;		// don't go too far overhead
    }
    AngleVectors( focusAngles, forward, NULL, NULL );

    VectorMA( cg.refdef.vieworg, FOCUS_DISTANCE, forward, focusPoint );

    VectorCopy( cg.refdef.vieworg, view );

    view[2] += 8;

    cg.refdef.viewangles[PITCH] *= 0.5;

    AngleVectors( cg.refdef.viewangles, forward, right, up );

    forwardScale = cos( cg_thirdPersonAngle.value / 180 * M_PI );
    sideScale = sin( cg_thirdPersonAngle.value / 180 * M_PI );
    VectorMA( view, -cg_thirdPersonRange.value * forwardScale, forward, view );
    VectorMA( view, -cg_thirdPersonRange.value * sideScale, right, view );

    // trace a ray from the origin to the viewpoint to make sure the view isn't
    // in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything

    if (!cg_cameraMode.integer) {
        CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID );

        if ( trace.fraction != 1.0 ) {
            VectorCopy( trace.endpos, view );
            view[2] += (1.0 - trace.fraction) * 32;
            // try another trace to this position, because a tunnel may have the ceiling
            // close enogh that this is poking out

            CG_Trace( &trace, cg.refdef.vieworg, mins, maxs, view, cg.predictedPlayerState.clientNum, MASK_SOLID );
            VectorCopy( trace.endpos, view );
        }
    }


    VectorCopy( view, cg.refdef.vieworg );

    // select pitch to look at focus point from vieword
    VectorSubtract( focusPoint, cg.refdef.vieworg, focusPoint );
    focusDist = sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
    if ( focusDist < 1 ) {
        focusDist = 1;	// should never happen
    }
    cg.refdef.viewangles[PITCH] = -180 / M_PI * atan2( focusPoint[2], focusDist );
    cg.refdef.viewangles[YAW] -= cg_thirdPersonAngle.value;
}


// this causes a compiler bug on mac MrC compiler
static void CG_StepOffset( void ) {
    int		timeDelta;
    
    // smooth out stair climbing
    timeDelta = cg.time - cg.stepTime;
    if ( timeDelta < STEP_TIME ) {
        cg.refdef.vieworg[2] -= cg.stepChange 
            * (STEP_TIME - timeDelta) / STEP_TIME;
    }
}*/

/*
===============
CG_OffsetFirstPersonView
===============
*/
static void CG_OffsetFirstPersonView(void)
{
    float *origin;
    float *angles;
    float bob;
    float ratio;
    float delta;
    float speed;
    float f;
    vec3_t predictedVelocity;
    int timeDelta;

    if (cg.snap->ps.pm_type == PM_INTERMISSION)
    {
        return;
    }

    origin = cg.refdef.vieworg;
    angles = cg.refdef.viewangles;

    // If dead, fall to the ground
    if (cg.snap->ps.pm_type == PM_DEAD)
    {
        int atime;
        int htime;
        float f;

        atime = (cg.time - cg.deathTime);
        if (atime > 450)
        {
            atime = 450;
        }

        htime = (cg.time - cg.deathTime);
        if (htime > 350)
        {
            htime = 350;
        }

        f = (float)atime / 450.0f;
        cg.refdef.viewangles[ROLL] += ((cg.deathTime & 0x100) ? -1 : 1) * 40 * f;
        cg.refdef.viewangles[PITCH] -= ((cg.deathTime & 0x010) ? -1 : 1) * 20 * f;
        cg.refdef.viewangles[YAW] -= ((cg.deathTime & 0x001) ? -1 : 1) * 40 * f;

        f = (float)htime / 400.0f;
        cg.refdef.vieworg[2] += (f * cg.predictedPlayerState.viewheight * 0.90f);

        return;
    }

    // add angles based on damage kick
    if (cg.damageTime)
    {
        ratio = cg.time - cg.damageTime;
        if (ratio < DAMAGE_DEFLECT_TIME)
        {
            ratio /= DAMAGE_DEFLECT_TIME;
            angles[PITCH] += ratio * cg.v_dmg_pitch;

            if (cg.v_dmg_roll != 255)
            {
                angles[ROLL] += ratio * cg.v_dmg_roll;
            }
        }
        else
        {
            ratio = 1.0 - (ratio - DAMAGE_DEFLECT_TIME) / DAMAGE_RETURN_TIME;
            if (ratio > 0)
            {
                angles[PITCH] += ratio * cg.v_dmg_pitch;

                if (cg.v_dmg_roll != 255)
                {
                    angles[ROLL] += ratio * cg.v_dmg_roll;
                }
            }
        }
    }

    // add pitch based on fall kick
#if 0
    ratio = ( cg.time - cg.landTime) / FALL_TIME;
    if (ratio < 0)
        ratio = 0;
    angles[PITCH] += ratio * cg.fall_value;
#endif

    // add angles based on velocity
    VectorCopy(cg.predictedPlayerState.velocity, predictedVelocity);

    delta = DotProduct(predictedVelocity, cg.refdef.viewaxis[0]);
    angles[PITCH] += delta * cg_runpitch.value;

    delta = DotProduct(predictedVelocity, cg.refdef.viewaxis[1]);
    angles[ROLL] -= delta * cg_runroll.value;

    // add angles based on bob

    // make sure the bob is visible even at low speeds
    speed = cg.xyspeed > 200 ? cg.xyspeed : 200;

    delta = cg.bobfracsin * cg_bobpitch.value * speed;
    if ((cg.predictedPlayerState.pm_flags & PMF_DUCKED) && (cg.predictedPlayerState.groundEntityNum != ENTITYNUM_NONE))
        delta *= 3; // crouching
    angles[PITCH] += delta;
    delta = cg.bobfracsin * cg_bobroll.value * speed;
    if ((cg.predictedPlayerState.pm_flags & PMF_DUCKED) && (cg.predictedPlayerState.groundEntityNum != ENTITYNUM_NONE))
        delta *= 3; // crouching accentuates roll
    if (cg.bobcycle & 1)
        delta = -delta;
    angles[ROLL] += delta;

    //===================================

    // add view height
    origin[2] += cg.predictedPlayerState.viewheight;

    // smooth out duck height changes
    timeDelta = cg.time - cg.duckTime;
    if (timeDelta < DUCK_TIME)
    {
        cg.refdef.vieworg[2] -= cg.duckChange * (DUCK_TIME - timeDelta) / DUCK_TIME;
    }

    // add bob height
    bob = cg.bobfracsin * cg.xyspeed * cg_bobup.value;
    if (bob > 6)
    {
        bob = 6;
    }

    origin[2] += bob;

    // add fall height
    delta = cg.time - cg.landTime;
    if (delta < LAND_DEFLECT_TIME)
    {
        f = delta / LAND_DEFLECT_TIME;
        cg.refdef.vieworg[2] += cg.landChange * f;
    }
    else if (delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME)
    {
        delta -= LAND_DEFLECT_TIME;
        f = 1.0 - (delta / LAND_RETURN_TIME);
        cg.refdef.vieworg[2] += cg.landChange * f;
    }

    // add step offset
    CG_StepOffset();

    // add kick offset

    if (cg.predictedPlayerState.pm_flags & PMF_LEANING)
    {
        float leanOffset;

        leanOffset = (float)(cg.predictedPlayerState.leanTime - LEAN_TIME) / LEAN_TIME * LEAN_OFFSET;
        angles[ROLL] += leanOffset / 4;

        BG_ApplyLeanOffset(&cg.predictedPlayerState, origin);
    }

    // Make sure view doesnt invert on itself
    angles[PITCH] = Com_Clampf(-89, 89, angles[PITCH]);
}

/*
====================
CG_CalcFovFromX

Calcs Y FOV from given X FOV
====================
*/
#define WAVE_AMPLITUDE 1
#define WAVE_FREQUENCY 0.4

qboolean CG_CalcFOVFromX(float fov_x)
{
    float x;
    //	float	phase;
    //	float	v;
    //	int		contents;
    float fov_y;
    qboolean inwater;

    x = cg.refdef.width / tan(fov_x / 360 * M_PI);
    fov_y = atan2(cg.refdef.height, x);
    fov_y = fov_y * 360 / M_PI;

    // there's a problem with this, it only takes the leafbrushes into account, not the entity brushes,
    //	so if you give slime/water etc properties to a func_door area brush in order to move the whole water
    //	level up/down this doesn't take into account the door position, so warps the view the whole time
    //	whether the water is up or not. Fortunately there's only one slime area in Trek that you can be under,
    //	so lose it...
#if 0
/*
    // warp if underwater
    contents = CG_PointContents( cg.refdef.vieworg, -1 );
    if ( contents & ( CONTENTS_WATER | CONTENTS_LAVA ) ){
        phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
        v = WAVE_AMPLITUDE * sin( phase );
        fov_x += v;
        fov_y -= v;
        inwater = qtrue;
    }
    else {
        inwater = qfalse;
    }
*/
#else
    inwater = qfalse;
#endif

    // set it
    cg.refdef.fov_x = fov_x;
    cg.refdef.fov_y = fov_y;

    return (inwater);
}

/*
====================
CG_CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
#define WAVE_AMPLITUDE 1
#define WAVE_FREQUENCY 0.4

static int CG_CalcFov(void)
{
    float x;
    float phase;
    float v;
    int contents;
    float fov_x, fov_y;
    float zoomFov;
    float f;
    int inwater;

    // OSP
    //if (cg.predictedPlayerState.pm_type == PM_INTERMISSION)
    if (cg.predictedPlayerState.pm_type == PM_INTERMISSION || cg.intermissionStarted == 2)
    // END
    {
        // if in intermission, use a fixed value
        fov_x = 90;
    }
    else
    {
        // user selectable
        if (cgs.dmflags & DF_FIXED_FOV)
        {
            // dmflag to prevent wide fov for all clients
            fov_x = 90;
        }
        else
        {
            fov_x = cg_fov.value;
            if (fov_x < 1)
            {
                fov_x = 1;
            }
            else if (fov_x > 160)
            {
                fov_x = 160;
            }
        }

        // If cheats arent enabled then 80 is the lowest and 100 is the highest
        if (!cg.cheats)
        {
            if (fov_x < 80)
            {
                fov_x = 80;
            }
            else if (fov_x > 100)
            {
                fov_x = 100;
            }
        }

        if (cg.predictedPlayerState.pm_flags & PMF_ZOOMED)
        {
            zoomFov = (float)weaponData[cg.predictedPlayerState.weapon].zoom[cg.predictedPlayerState.zoomFov].fov;

            if (!cg.predictedPlayerState.pm_flags & PMF_ZOOM_LOCKED)
            {
                zoomFov -= cg.frametime * 0.05f;

                if (zoomFov < MAX_ZOOM_FOV)
                {
                    zoomFov = MAX_ZOOM_FOV;
                }
                else if (zoomFov > cg_fov.value)
                {
                    zoomFov = cg_fov.value;
                }
                else
                { // Still zooming
                    static zoomSoundTime = 0;

                    if (zoomSoundTime < cg.time || zoomSoundTime > cg.time + 10000)
                    {
                        zoomSoundTime = cg.time + 300;
                    }
                }
            }

            fov_x = zoomFov;
        }
        else
        {
            zoomFov = (float)weaponData[cg.predictedPlayerState.weapon].zoom[cg.predictedPlayerState.zoomFov].fov;

            f = (cg.time - cg.predictedPlayerState.zoomTime) / ZOOM_OUT_TIME;
            if (f > 1.0)
            {
                fov_x = fov_x;
            }
            else
            {
                fov_x = zoomFov + f * (fov_x - zoomFov);
            }
        }
    }

    x = cg.refdef.width / tan(fov_x / 360 * M_PI);
    fov_y = atan2(cg.refdef.height, x);
    fov_y = fov_y * 360 / M_PI;

    // warp if underwater
    contents = CG_PointContents(cg.refdef.vieworg, -1);
    if (contents & (CONTENTS_WATER | CONTENTS_LAVA))
    {
        phase = cg.time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
        v = WAVE_AMPLITUDE * sin(phase);
        fov_x += v;
        fov_y -= v;
        inwater = qtrue;
    }
    else
    {
        inwater = qfalse;
    }

    // set it
    cg.refdef.fov_x = fov_x;
    cg.refdef.fov_y = fov_y;

    if (!(cg.predictedPlayerState.pm_flags & PMF_ZOOMED))
    {
        cg.zoomSensitivity = 1;
    }
    else
    {
        cg.zoomSensitivity = cg.refdef.fov_y / 75.0;
    }

    return inwater;
}

static void CG_DamageBlendBlob(void)
{
    int t = 250;
    int maxTime = DAMAGE_TIME;
    vec3_t lineStart;
    vec3_t lineEnd;
    vec3_t rgb1;
    vec3_t rgb2;
    qboolean dmgIsToTheLeft;
    qboolean dmgIsAbove;
    qboolean unknownDir;
    float alphaVal;
    float cvarScale;

    if (cg_damageindicator.value <= 0)
    {
        return;
    }

    t = cg.time - cg.damageTime;
    if (t <= 0 || t >= maxTime)
    {
        return;
    }

    // here we use cg.damageY as a value to indicate how much above you the damage is and
    //cg.damageX as a value to indicate how much to your left the damage is

    // draw a "line" using the cgs.media.damageDirShader
    dmgIsToTheLeft = (cg.damageX > 0);
    dmgIsAbove = (cg.damageY < 0);
    unknownDir = (cg.v_dmg_roll == 255);
    alphaVal = (1.0 - ((float)t / maxTime));
    cvarScale = 1.0f * cg_damageindicator.value;

    VectorSet(rgb1, 1, 1, 1);
    VectorSet(rgb2, 1, 1, 1);

    // draw the left/right indicator if it's far enough from center
    if ((fabs(cg.damageX) > 25) || unknownDir)
    {
        VectorMA(cg.refdef.vieworg, 20, cg.refdef.viewaxis[0], lineStart);
        VectorMA(lineStart, (dmgIsToTheLeft ? 6 : -6), cg.refdef.viewaxis[1], lineStart);
        VectorMA(lineStart, (dmgIsToTheLeft ? 2 : -2) * cvarScale, cg.refdef.viewaxis[1], lineEnd);

        trap_FX_AddLine(lineStart, lineEnd,
                        5.0f * cvarScale, 5.0f * cvarScale, 0.0f,
                        alphaVal, alphaVal, 0.0f,
                        rgb1, rgb2, 0.0f,
                        1, cgs.media.damageDirShader, FX_DEPTH_HACK);
    }
    if (unknownDir)
    {
        // dmg from unknown direction...display all four directions
        VectorMA(cg.refdef.vieworg, 20, cg.refdef.viewaxis[0], lineStart);
        VectorMA(lineStart, (dmgIsToTheLeft ? -6 : 6), cg.refdef.viewaxis[1], lineStart);
        VectorMA(lineStart, (dmgIsToTheLeft ? -2 : 2) * cvarScale, cg.refdef.viewaxis[1], lineEnd);

        trap_FX_AddLine(lineStart, lineEnd,
                        5.0f * cvarScale, 5.0f * cvarScale, 0.0f,
                        alphaVal, alphaVal, 0.0f,
                        rgb1, rgb2, 0.0f,
                        1, cgs.media.damageDirShader, FX_DEPTH_HACK);
    }

    // draw the above/below indicator
    if ((fabs(cg.damageY) > 15) || unknownDir)
    {
        VectorMA(cg.refdef.vieworg, 20, cg.refdef.viewaxis[0], lineStart);
        VectorMA(lineStart, (dmgIsAbove ? 6 : -6), cg.refdef.viewaxis[2], lineStart);
        VectorMA(lineStart, (dmgIsAbove ? 2 : -2) * cvarScale, cg.refdef.viewaxis[2], lineEnd);

        trap_FX_AddLine(lineStart, lineEnd,
                        5.0f * cvarScale, 5.0f * cvarScale, 0.0f,
                        alphaVal, alphaVal, 0.0f,
                        rgb1, rgb2, 0.0f,
                        1, cgs.media.damageDirShader, FX_DEPTH_HACK);
    }
    if (unknownDir)
    {
        VectorMA(cg.refdef.vieworg, 20, cg.refdef.viewaxis[0], lineStart);
        VectorMA(lineStart, (dmgIsAbove ? -6 : 6), cg.refdef.viewaxis[2], lineStart);
        VectorMA(lineStart, (dmgIsAbove ? -2 : 2) * cvarScale, cg.refdef.viewaxis[2], lineEnd);

        trap_FX_AddLine(lineStart, lineEnd,
                        5.0f * cvarScale, 5.0f * cvarScale, 0.0f,
                        alphaVal, alphaVal, 0.0f,
                        rgb1, rgb2, 0.0f,
                        1, cgs.media.damageDirShader, FX_DEPTH_HACK);
    }
}

/*
===============
CG_CalcViewValues

Sets cg.refdef view values
===============
*/
static int CG_CalcViewValues(void)
{
    playerState_t *ps;

    memset(&cg.refdef, 0, sizeof(cg.refdef));

    // strings for in game rendering
    // Q_strncpyz( cg.refdef.text[0], "Park Ranger", sizeof(cg.refdef.text[0]) );
    // Q_strncpyz( cg.refdef.text[1], "19", sizeof(cg.refdef.text[1]) );

    // calculate size of 3D view
    CG_CalcVrect();

    ps = &cg.predictedPlayerState;
    /*
    if (cg.cameraMode) {
        vec3_t origin, angles;
        if (trap_getCameraInfo(cg.time, &origin, &angles)) {
            VectorCopy(origin, cg.refdef.vieworg);
            angles[ROLL] = 0;
            VectorCopy(angles, cg.refdef.viewangles);
            AnglesToAxis( cg.refdef.viewangles, cg.refdef.viewaxis );
            return CG_CalcFov();
        } else {
            cg.cameraMode = qfalse;
        }
    }
*/
    // intermission view
    if (ps->pm_type == PM_INTERMISSION)
    {
        VectorCopy(ps->origin, cg.refdef.vieworg);
        VectorCopy(ps->viewangles, cg.refdef.viewangles);
        AnglesToAxis(cg.refdef.viewangles, cg.refdef.viewaxis);
        return CG_CalcFov();
    }

    cg.bobcycle = (ps->bobCycle & 128) >> 7;
    cg.bobfracsin = fabs(sin((ps->bobCycle & 127) / 127.0 * M_PI));
    cg.xyspeed = sqrt(ps->velocity[0] * ps->velocity[0] +
                      ps->velocity[1] * ps->velocity[1]);

    VectorCopy(ps->origin, cg.refdef.vieworg);
    VectorCopy(ps->viewangles, cg.refdef.viewangles);

    if (cg_cameraOrbit.integer)
    {
        if (cg.time > cg.nextOrbitTime)
        {
            cg.nextOrbitTime = cg.time + cg_cameraOrbitDelay.integer;
            cg_thirdPersonYaw.value += cg_cameraOrbit.value;
        }
    }

    // add error decay
    if (cg_errorDecay.value > 0)
    {
        int t;
        float f;

        t = cg.time - cg.predictedErrorTime;
        f = (cg_errorDecay.value - t) / cg_errorDecay.value;
        if (f > 0 && f < 1)
        {
            VectorMA(cg.refdef.vieworg, f, cg.predictedError, cg.refdef.vieworg);
        }
        else
        {
            cg.predictedErrorTime = 0;
        }
    }

    if (cg.renderingThirdPerson && !(cg.snap->ps.pm_flags & PMF_ZOOMED))
    {
        // back away from character
        CG_OffsetThirdPersonView();
    }
    else
    {
        // offset for local bobbing and kicks
        CG_OffsetFirstPersonView();
    }

    CG_UpdateCameraShake(cg.refdef.vieworg, cg.refdef.viewangles);

    // position eye reletive to origin
    AnglesToAxis(cg.refdef.viewangles, cg.refdef.viewaxis);

    if (cg.hyperspace)
    {
        cg.refdef.rdflags |= RDF_NOWORLDMODEL | RDF_HYPERSPACE;
    }

    // field of view
    return CG_CalcFov();
}

/*
=====================
CG_AddBufferedSound
=====================
*/
void CG_AddBufferedSound(sfxHandle_t sfx)
{
    if (!sfx)
        return;
    cg.soundBuffer[cg.soundBufferIn] = sfx;
    cg.soundBufferIn = (cg.soundBufferIn + 1) % MAX_SOUNDBUFFER;
    if (cg.soundBufferIn == cg.soundBufferOut)
    {
        cg.soundBufferOut++;
    }
}

/*
=====================
CG_PlayBufferedSounds
=====================
*/
static void CG_PlayBufferedSounds(void)
{
    if (cg.soundTime < cg.time)
    {
        if (cg.soundBufferOut != cg.soundBufferIn && cg.soundBuffer[cg.soundBufferOut])
        {
            trap_S_StartLocalSound(cg.soundBuffer[cg.soundBufferOut], CHAN_ANNOUNCER);
            cg.soundBuffer[cg.soundBufferOut] = 0;
            cg.soundBufferOut = (cg.soundBufferOut + 1) % MAX_SOUNDBUFFER;
            cg.soundTime = cg.time + 750;
        }
    }
}

//=========================================================================

/*
=================
CG_UpdateRogerWilco

Updates the roger wilco team you are on
=================
*/
void CG_UpdateRogerWilco(void)
{
    qboolean dead = qfalse;

    if (!rw_enabled.integer)
    {
        return;
    }

    if (cgs.clientinfo[cg.clientNum].team != TEAM_SPECTATOR)
    {
        if (cgs.gametypeData->respawnType == RT_NONE)
        {
            if (cg.predictedPlayerState.pm_flags & (PMF_GHOST | PMF_FOLLOW) || cg.predictedPlayerState.pm_type == PM_DEAD)
            {
                dead = qtrue;
            }
        }
    }

    trap_RW_SetTeam(cgs.clientinfo[cg.clientNum].team, dead);
}

/*
=================
CG_DrawActiveFrame

Generates and draws a game scene and status information at the given time.
=================
*/
void CG_DrawActiveFrame(int serverTime, stereoFrame_t stereoView, qboolean demoPlayback)
{
    int inwater;

    cg.time = serverTime;
    cg.demoPlayback = demoPlayback;

    // update cvars
    CG_UpdateCvars();

    // if we are only updating the screen as a loading
    // pacifier, don't even try to read snapshots
    if (cg.infoScreenText[0] != 0)
    {
        CG_DrawInformation();
        return;
    }

    trap_FX_AdjustTime(cg.time);

    CG_UpdateRogerWilco();

    CG_RunLightStyles();

    // any looped sounds will be respecified as entities
    // are added to the render list
    trap_S_ClearLoopingSounds(qfalse);

    // clear all the render lists
    trap_R_ClearScene();

    // set up cg.snap and possibly cg.nextSnap
    CG_ProcessSnapshots();

    // if we haven't received any snapshots yet, all
    // we can draw is the information screen
    if (!cg.snap || (cg.snap->snapFlags & SNAPFLAG_NOT_ACTIVE))
    {
        CG_DrawInformation();
        return;
    }

    // Handle last weapon selection
    if (cg.weaponSelect != cg.weaponOldSelect)
    {
        cg.weaponLastSelect = cg.weaponOldSelect;
        cg.weaponOldSelect = cg.weaponSelect;
    }

    // let the client system know what our weapon and zoom settings are
    if (cg.weaponMenuUp)
    {
        trap_SetUserCmdValue((cg.weaponMenuSelect | WP_DELAYED_CHANGE_BIT), cg.zoomSensitivity);
    }
    else
    {
        trap_SetUserCmdValue(cg.weaponSelect, cg.zoomSensitivity);
    }

    // this counter will be bumped for every valid scene we generate
    cg.clientFrame++;

    // update cg.predictedPlayerState
    CG_PredictPlayerState();

    // update the scores from the playerstate
    if (cgs.scores1 != cg.predictedPlayerState.persistant[PERS_RED_SCORE])
    {
        cgs.scores1 = cg.predictedPlayerState.persistant[PERS_RED_SCORE];
        trap_Cvar_Set("ui_info_redscore", va("%d", cgs.scores1));
    }

    if (cgs.scores2 != cg.predictedPlayerState.persistant[PERS_BLUE_SCORE])
    {
        cgs.scores2 = cg.predictedPlayerState.persistant[PERS_BLUE_SCORE];
        trap_Cvar_Set("ui_info_bluescore", va("%d", cgs.scores2));
    }

    // decide on third person view
    if (cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR || (cg.snap->ps.pm_flags & PMF_GHOST))
    {
        cg.renderingThirdPerson = qfalse;
    }
    else if (cg.snap->ps.pm_type == PM_DEAD)
    {
        cg.renderingThirdPerson = cg_thirdPerson.integer;
    }
    else
    {
        if (cg.snap->ps.pm_flags & PMF_ZOOMED)
        {
            cg.renderingThirdPerson = qfalse;
        }
        else if (cg.snap->ps.pm_flags & PMF_FOLLOW)
        {
            if (cg.snap->ps.pm_flags & PMF_FOLLOWFIRST)
            {
                cg.renderingThirdPerson = qfalse;
            }
            else
            {
                cg.renderingThirdPerson = qtrue;
            }
        }
        else
        {
            cg.renderingThirdPerson = cg_thirdPerson.integer || (cg.snap->ps.stats[STAT_HEALTH] <= 0);
        }
    }

    // build cg.refdef
    inwater = CG_CalcViewValues();

    // first person blend blobs, done after AnglesToAxis
    if (!cg.renderingThirdPerson)
    {
        CG_DamageBlendBlob();
    }

    // Load deferred models
    if (cg.deferredPlayerLoading > 10)
    {
        CG_LoadDeferredPlayers();
        cg.deferredPlayerLoading = 0;
    }

    // build the render lists
    if (!cg.hyperspace)
    {
        CG_AddPacketEntities(); // adter calcViewValues, so predicted player state is correct
        CG_AddLocalEntities();
        CG_DrawMiscEnts();
    }

    if (cg.snap->ps.stats[STAT_HEALTH] > 0)
    {
        CG_AnimateViewWeapon(&cg.predictedPlayerState);
        CG_AddViewWeapon(&cg.predictedPlayerState);
    }

    if (!cg.hyperspace)
    {
        trap_FX_AddScheduledEffects();
    }

    // add buffered sounds
    CG_PlayBufferedSounds();

    // play buffered voice chats
    CG_PlayBufferedVoiceChats();

    // finish up the rest of the refdef
    if (cg.testModelEntity.hModel)
    {
        CG_AddTestModel();
    }
    cg.refdef.time = cg.time;
    memcpy(cg.refdef.areamask, cg.snap->areamask, sizeof(cg.refdef.areamask));

    // update audio positions
    if (!cg.mMapChange)
    {
        trap_AS_UpdateAmbientSet(CG_ConfigString(CS_AMBIENT_SOUNDSETS), cg.refdef.vieworg); // MUST be before trap_S_Respatialize()!	-ste
        trap_S_Respatialize(cg.snap->ps.clientNum, cg.refdef.vieworg, cg.refdef.viewaxis, inwater);
    }

    // make sure the lagometerSample and frame timing isn't done twice when in stereo
    if (stereoView != STEREO_RIGHT)
    {
        cg.frametime = cg.time - cg.oldTime;
        if (cg.frametime < 0)
        {
            cg.frametime = 0;
        }
        cg.oldTime = cg.time;
        CG_AddLagometerFrameInfo();
    }

    // actually issue the rendering calls
    CG_DrawActive(stereoView);

    if (cg_stats.integer)
    {
        Com_Printf("cg.clientFrame:%i\n", cg.clientFrame);
    }

    // OSP
    sub_00027c25();
    // END
}

/*
=================
CG_CameraShake

Shakes the camera a bit (used in grenades)
=================
*/
#define MAX_SHAKE_INTENSITY 2.0f
#define DEFAULT_EXPLOSION_RADIUS 400

void CG_CameraShake(float *origin, float intensity, int radius, int time)
{
    vec3_t dir;
    float dist, intensityScale;
    float realIntensity;

    VectorSubtract(cg.refdef.vieworg, origin, dir);
    dist = VectorNormalize(dir);

    // Apparently the SoF2 camera shake function takes a time, but not a radius.
    //	when someone feels like fixing this, we can be a bit more flexible
    radius = DEFAULT_EXPLOSION_RADIUS;

    //Use the dir to add kick to the explosion
    if (dist > radius)
        return;

    intensityScale = 1 - (dist / (float)radius);
    realIntensity = intensity * intensityScale;

    if (realIntensity > MAX_SHAKE_INTENSITY)
        realIntensity = MAX_SHAKE_INTENSITY;

    cg.shakeIntensity = realIntensity;
    cg.shakeDuration = time;
    cg.shakeStart = cg.time;
}

/*
=================
CG_UpdateCameraShake

Updates the camera shake
=================
*/
void CG_UpdateCameraShake(vec3_t origin, vec3_t angles)
{
    vec3_t moveDir;
    float intensity_scale;
    float intensity;
    int i;

    if (cg.shakeDuration <= 0)
        return;

    if (cg.time > (cg.shakeStart + cg.shakeDuration))
    {
        cg.shakeIntensity = 0;
        cg.shakeDuration = 0;
        cg.shakeStart = 0;
        return;
    }

    //intensity_scale now also takes into account FOV with 90.0 as normal
    intensity_scale = 1.0f - ((float)(cg.time - cg.shakeStart) / (float)cg.shakeDuration) * (((cg.refdef.fov_x + cg.refdef.fov_y) / 2.0f) / 90.0f);
    intensity = cg.shakeIntensity * intensity_scale;

    for (i = 0; i < 3; i++)
    {
        moveDir[i] = flrand(-intensity, intensity);
    }

    //FIXME: Lerp

    //Move the camera
    VectorAdd(origin, moveDir, origin);

    for (i = 0; i < 3; i++)
    {
        moveDir[i] = flrand(-intensity, intensity);
    }

    //FIXME: Lerp

    //Move the angles
    VectorAdd(angles, moveDir, angles);
}
