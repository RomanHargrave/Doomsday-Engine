/** @file viewports.cpp  Player viewports and related low-level rendering.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "render/viewports.h"

#include <QBitArray>
#include <de/concurrency.h>
#include <de/timer.h>
#include <de/vector1.h>
#include <de/GLState>
#include <doomsday/filesys/fs_util.h>

#include "clientapp.h"
#include "api_console.h"
#include "dd_main.h"
#include "dd_loop.h"
#include "edit_bias.h"

#include "gl/gl_main.h"

#include "api_render.h"
#include "render/r_draw.h"
#include "render/r_main.h"
#include "render/fx/bloom.h"
#include "render/angleclipper.h"
#include "render/cameralensfx.h"
#include "render/rendpoly.h"
#include "render/skydrawable.h"
#include "render/vissprite.h"
#include "render/vr.h"

#include "network/net_demo.h"

#include "world/linesighttest.h"
#include "world/thinkers.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/sky.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "SectorCluster"
#include "Surface"
#include "Contact"

#include "ui/ui_main.h"
#include "ui/clientwindow.h"
#include "ui/widgets/gameuiwidget.h"

using namespace de;

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
dfloat devCameraMovementStartTime;          ///< sysTime
dfloat devCameraMovementStartTimeRealSecs;
#endif

dd_bool firstFrameAfterLoad;

static dint loadInStartupMode;
static dint rendCameraSmooth = true;  ///< Smoothed by default.
static dbyte showFrameTimePos;
static dbyte showViewAngleDeltas;
static dbyte showViewPosDeltas;

dint rendInfoTris;

static viewport_t *currentViewport;

static coord_t *luminousDist;
static dbyte *luminousClipped;
static duint *luminousOrder;
static QBitArray subspacesVisible;

static QBitArray generatorsVisible(Map::MAX_GENERATORS);

static viewdata_t viewDataOfConsole[DDMAXPLAYERS];  ///< Indexed by console number.

static dint frameCount;

static dint gridCols, gridRows;
static viewport_t viewportOfLocalPlayer[DDMAXPLAYERS];

static dint resetNextViewer = true;

static inline RenderSystem &rendSys()
{
    return ClientApp::renderSystem();
}

static inline WorldSystem &worldSys()
{
    return ClientApp::worldSystem();
}

dint R_FrameCount()
{
    return frameCount;
}

void R_ResetFrameCount()
{
    frameCount = 0;
}

#undef R_SetViewOrigin
DENG_EXTERN_C void R_SetViewOrigin(dint consoleNum, coord_t const origin[3])
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.origin = Vector3d(origin);
}

#undef R_SetViewAngle
DENG_EXTERN_C void R_SetViewAngle(dint consoleNum, angle_t angle)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.setAngle(angle);
}

#undef R_SetViewPitch
DENG_EXTERN_C void R_SetViewPitch(dint consoleNum, dfloat pitch)
{
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;
    viewDataOfConsole[consoleNum].latest.pitch = pitch;
}

void R_SetupDefaultViewWindow(dint consoleNum)
{
    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS) return;

    vd->window =
        vd->windowOld =
            vd->windowTarget = Rectanglei::fromSize(Vector2i(0, 0), Vector2ui(DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT));
    vd->windowInter = 1;
}

void R_ViewWindowTicker(dint consoleNum, timespan_t ticLength)
{
    viewdata_t *vd = &viewDataOfConsole[consoleNum];
    if(consoleNum < 0 || consoleNum >= DDMAXPLAYERS)
    {
        return;
    }

    vd->windowInter += dfloat(.4 * ticLength * TICRATE);
    if(vd->windowInter >= 1)
    {
        vd->window = vd->windowTarget;
    }
    else
    {
        vd->window.moveTopLeft(Vector2i(de::roundf(de::lerp<dfloat>(vd->windowOld.topLeft.x, vd->windowTarget.topLeft.x, vd->windowInter)),
                                        de::roundf(de::lerp<dfloat>(vd->windowOld.topLeft.y, vd->windowTarget.topLeft.y, vd->windowInter))));
        vd->window.setSize(Vector2ui(de::roundf(de::lerp<dfloat>(vd->windowOld.width(),  vd->windowTarget.width(),  vd->windowInter)),
                                     de::roundf(de::lerp<dfloat>(vd->windowOld.height(), vd->windowTarget.height(), vd->windowInter))));
    }
}

#undef R_ViewWindowGeometry
DENG_EXTERN_C dint R_ViewWindowGeometry(dint player, RectRaw *geometry)
{
    if(!geometry) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const &vd = viewDataOfConsole[player];
    geometry->origin.x    = vd.window.topLeft.x;
    geometry->origin.y    = vd.window.topLeft.y;
    geometry->size.width  = vd.window.width();
    geometry->size.height = vd.window.height();
    return true;
}

#undef R_ViewWindowOrigin
DENG_EXTERN_C dint R_ViewWindowOrigin(dint player, Point2Raw *origin)
{
    if(!origin) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const &vd = viewDataOfConsole[player];
    origin->x = vd.window.topLeft.x;
    origin->y = vd.window.topLeft.y;
    return true;
}

#undef R_ViewWindowSize
DENG_EXTERN_C dint R_ViewWindowSize(dint player, Size2Raw *size)
{
    if(!size) return false;
    if(player < 0 || player >= DDMAXPLAYERS) return false;

    viewdata_t const &vd = viewDataOfConsole[player];
    size->width  = vd.window.width();
    size->height = vd.window.height();
    return true;
}

/**
 * @note Do not change values used during refresh here because we might be
 * partway through rendering a frame. Changes should take effect on next
 * refresh only.
 */
#undef R_SetViewWindowGeometry
DENG_EXTERN_C void R_SetViewWindowGeometry(dint player, RectRaw const *geometry, dd_bool interpolate)
{
    dint p = P_ConsoleToLocal(player);
    if(p < 0) return;

    viewport_t const *vp = &viewportOfLocalPlayer[p];
    viewdata_t *vd = &viewDataOfConsole[player];

    Rectanglei newGeom = Rectanglei::fromSize(Vector2i(de::clamp<dint>(0, geometry->origin.x, vp->geometry.width()),
                                                       de::clamp<dint>(0, geometry->origin.y, vp->geometry.height())),
                                              Vector2ui(de::abs(geometry->size.width),
                                                        de::abs(geometry->size.height)));

    if((unsigned) newGeom.bottomRight.x > vp->geometry.width())
    {
        newGeom.setWidth(vp->geometry.width() - newGeom.topLeft.x);
    }
    if((unsigned) newGeom.bottomRight.y > vp->geometry.height())
    {
        newGeom.setHeight(vp->geometry.height() - newGeom.topLeft.y);
    }

    // Already at this target?
    if(vd->window == newGeom)
    {
        return;
    }

    // Record the new target.
    vd->windowTarget = newGeom;

    // Restart or advance the interpolation timer?
    // If dimensions have not yet been set - do not interpolate.
    if(interpolate && vd->window.size() != Vector2ui(0, 0))
    {
        vd->windowOld   = vd->window;
        vd->windowInter = 0;
    }
    else
    {
        vd->windowOld   = vd->windowTarget;
        vd->windowInter = 1;  // Update on next frame.
    }
}

#undef R_ViewPortGeometry
DENG_EXTERN_C dint R_ViewPortGeometry(dint player, RectRaw *geometry)
{
    if(!geometry) return false;

    dint p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t const &vp = viewportOfLocalPlayer[p];
    geometry->origin.x    = vp.geometry.topLeft.x;
    geometry->origin.y    = vp.geometry.topLeft.y;
    geometry->size.width  = vp.geometry.width();
    geometry->size.height = vp.geometry.height();
    return true;
}

#undef R_ViewPortOrigin
DENG_EXTERN_C dint R_ViewPortOrigin(dint player, Point2Raw *origin)
{
    if(!origin) return false;

    dint p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t const &vp = viewportOfLocalPlayer[p];
    origin->x = vp.geometry.topLeft.x;
    origin->y = vp.geometry.topLeft.y;
    return true;
}

#undef R_ViewPortSize
DENG_EXTERN_C dint R_ViewPortSize(dint player, Size2Raw *size)
{
    if(!size) return false;

    dint p = P_ConsoleToLocal(player);
    if(p == -1) return false;

    viewport_t const &vp = viewportOfLocalPlayer[p];
    size->width  = vp.geometry.width();
    size->height = vp.geometry.height();
    return true;
}

#undef R_SetViewPortPlayer
DENG_EXTERN_C void R_SetViewPortPlayer(dint consoleNum, dint viewPlayer)
{
    dint p = P_ConsoleToLocal(consoleNum);
    if(p != -1)
    {
        viewportOfLocalPlayer[p].console = viewPlayer;
    }
}

/**
 * Calculate the placement and dimensions of a specific viewport.
 * Assumes that the grid has already been configured.
 */
void R_UpdateViewPortGeometry(viewport_t *port, dint col, dint row)
{
    DENG2_ASSERT(port);

    Rectanglei newGeom = Rectanglei(Vector2i(DENG_GAMEVIEW_X + col * DENG_GAMEVIEW_WIDTH  / gridCols,
                                             DENG_GAMEVIEW_Y + row * DENG_GAMEVIEW_HEIGHT / gridRows),
                                    Vector2i(DENG_GAMEVIEW_X + (col+1) * DENG_GAMEVIEW_WIDTH  / gridCols,
                                             DENG_GAMEVIEW_Y + (row+1) * DENG_GAMEVIEW_HEIGHT / gridRows));
    ddhook_viewport_reshape_t p;

    if(port->geometry == newGeom) return;

    bool doReshape = false;
    if(port->console != -1 && Plug_CheckForHook(HOOK_VIEWPORT_RESHAPE))
    {
        p.oldGeometry.origin.x    = port->geometry.topLeft.x;
        p.oldGeometry.origin.y    = port->geometry.topLeft.y;
        p.oldGeometry.size.width  = port->geometry.width();
        p.oldGeometry.size.height = port->geometry.height();
        doReshape = true;
    }

    port->geometry = newGeom;

    if(doReshape)
    {
        p.geometry.origin.x    = port->geometry.topLeft.x;
        p.geometry.origin.y    = port->geometry.topLeft.y;
        p.geometry.size.width  = port->geometry.width();
        p.geometry.size.height = port->geometry.height();

        DD_CallHooks(HOOK_VIEWPORT_RESHAPE, port->console, (void *)&p);
    }
}

bool R_SetViewGrid(dint numCols, dint numRows)
{
    if(numCols > 0 && numRows > 0)
    {
        if(numCols * numRows > DDMAXPLAYERS)
        {
            return false;
        }

        if(numCols != gridCols || numRows != gridRows)
        {
            // The number of consoles has changes; LensFx needs to reallocate resources
            // only for the consoles in use.
            /// @todo This could be done smarter, only for the affected viewports. -jk
            LensFx_GLRelease();
        }

        if(numCols > DDMAXPLAYERS)
            numCols = DDMAXPLAYERS;
        if(numRows > DDMAXPLAYERS)
            numRows = DDMAXPLAYERS;

        gridCols = numCols;
        gridRows = numRows;
    }

    dint p = 0;
    for(dint y = 0; y < gridRows; ++y)
    for(dint x = 0; x < gridCols; ++x)
    {
        // The console number is -1 if the viewport belongs to no one.
        viewport_t *vp = &viewportOfLocalPlayer[p];

        dint const console = P_LocalToConsole(p);
        if(console != -1)
        {
            vp->console = clients[console].viewConsole;
        }
        else
        {
            vp->console = -1;
        }

        R_UpdateViewPortGeometry(vp, x, y);
        ++p;
    }

    return true;
}

void R_ResetViewer()
{
    resetNextViewer = 1;
}

dint R_NextViewer()
{
    return resetNextViewer;
}

viewdata_t const *R_ViewData(dint consoleNum)
{
    DENG2_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);
    return &viewDataOfConsole[consoleNum];
}

/**
 * The components whose difference is too large for interpolation will be
 * snapped to the sharp values.
 */
void R_CheckViewerLimits(viewer_t *src, viewer_t *dst)
{
    dint const MAXMOVE = 32;

    /// @todo Remove this snapping. The game should determine this and disable the
    ///       the interpolation as required.
    if(fabs(dst->origin.x - src->origin.x) > MAXMOVE ||
       fabs(dst->origin.y - src->origin.y) > MAXMOVE)
    {
        src->origin = dst->origin;
    }

    /*
    if(abs(dint(dst->angle) - dint(src->angle)) >= ANGLE_45)
    {
        LOG_DEBUG("R_CheckViewerLimits: Snap camera angle to %08x.") << dst->angle;
        src->angle = dst->angle;
    }
    */
}

/**
 * Retrieve the current sharp camera position.
 */
viewer_t R_SharpViewer(player_t &player)
{
    DENG2_ASSERT(player.shared.mo);

    ddplayer_t const &ddpl = player.shared;

    viewer_t view(viewDataOfConsole[&player - ddPlayers].latest);

    if((ddpl.flags & DDPF_CHASECAM) && !(ddpl.flags & DDPF_CAMERA))
    {
        // STUB
        // This needs to be fleshed out with a proper third person
        // camera control setup. Currently we simply project the viewer's
        // position a set distance behind the ddpl.
        dfloat const distance = 90;

        duint angle = view.angle() >> ANGLETOFINESHIFT;
        duint pitch = angle_t(LOOKDIR2DEG(view.pitch) / 360 * ANGLE_MAX) >> ANGLETOFINESHIFT;

        view.origin -= Vector3d(FIX2FLT(fineCosine[angle]),
                                FIX2FLT(finesine[angle]),
                                FIX2FLT(finesine[pitch])) * distance;
    }

    // Check that the viewZ doesn't go too high or low.
    // Cameras are not restricted.
    if(!(ddpl.flags & DDPF_CAMERA))
    {
        if(view.origin.z > ddpl.mo->ceilingZ - 4)
        {
            view.origin.z = ddpl.mo->ceilingZ - 4;
        }

        if(view.origin.z < ddpl.mo->floorZ + 4)
        {
            view.origin.z = ddpl.mo->floorZ + 4;
        }
    }

    return view;
}

void R_NewSharpWorld()
{
    if(resetNextViewer)
    {
        resetNextViewer = 2;
    }

    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        viewdata_t *vd = &viewDataOfConsole[i];
        player_t *plr  = &ddPlayers[i];

        if(/*(plr->shared.flags & DDPF_LOCAL) &&*/
           (!plr->shared.inGame || !plr->shared.mo))
        {
            continue;
        }

        viewer_t sharpView = R_SharpViewer(*plr);

        // The game tic has changed, which means we have an updated sharp
        // camera position.  However, the position is at the beginning of
        // the tic and we are most likely not at a sharp tic boundary, in
        // time.  We will move the viewer positions one step back in the
        // buffer.  The effect of this is that [0] is the previous sharp
        // position and [1] is the current one.

        vd->lastSharp[0] = vd->lastSharp[1];
        vd->lastSharp[1] = sharpView;

        R_CheckViewerLimits(vd->lastSharp, &sharpView);
    }

    if(worldSys().hasMap())
    {
        Map &map = worldSys().map();
        map.updateTrackedPlanes();
        map.updateScrollingSurfaces();
    }
}

void R_UpdateViewer(dint consoleNum)
{
    DENG2_ASSERT(consoleNum >= 0 && consoleNum < DDMAXPLAYERS);

    dint const VIEWPOS_MAX_SMOOTHDISTANCE = 172;

    viewdata_t *vd   = viewDataOfConsole + consoleNum;
    player_t *player = ddPlayers + consoleNum;

    if(!player->shared.inGame) return;
    if(!player->shared.mo) return;

    viewer_t sharpView = R_SharpViewer(*player);

    if(resetNextViewer ||
       (sharpView.origin - vd->current.origin).length() > VIEWPOS_MAX_SMOOTHDISTANCE)
    {
        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
        {
            resetNextViewer = 0;
        }

        // Just view from the sharp position.
        vd->current = sharpView;

        vd->lastSharp[0] = vd->lastSharp[1] = sharpView;
    }
    // While the game is paused there is no need to calculate any
    // time offsets or interpolated camera positions.
    else  //if(!clientPaused)
    {
        // Calculate the smoothed camera position, which is somewhere between
        // the previous and current sharp positions. This introduces a slight
        // delay (max. 1/35 sec) to the movement of the smoothed camera.
        viewer_t smoothView = vd->lastSharp[0].lerp(vd->lastSharp[1], frameTimePos);

        // Use the latest view angles known to us if the interpolation flags
        // are not set. The interpolation flags are used when the view angles
        // are updated during the sharp tics and need to be smoothed out here.
        // For example, view locking (dead or camera setlock).
        /*if(!(player->shared.flags & DDPF_INTERYAW))
            smoothView.angle = sharpView.angle;*/
        /*if(!(player->shared.flags & DDPF_INTERPITCH))
            smoothView.pitch = sharpView.pitch;*/

        vd->current = smoothView;

        // Monitor smoothness of yaw/pitch changes.
        if(showViewAngleDeltas)
        {
            struct OldAngle {
                ddouble time;
                dfloat yaw;
                dfloat pitch;
            };

            static OldAngle oldAngle[DDMAXPLAYERS];
            OldAngle *old = &oldAngle[viewPlayer - ddPlayers];
            dfloat yaw    = (ddouble)smoothView.angle() / ANGLE_MAX * 360;

            LOGDEV_MSG("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f "
                       "Rdx=%-10.3f Rdy=%-10.3f")
                    << SECONDS_TO_TICKS(gameTime)
                    << frameTimePos
                    << sysTime - old->time
                    << yaw - old->yaw
                    << smoothView.pitch - old->pitch
                    << (yaw - old->yaw) / (sysTime - old->time)
                    << (smoothView.pitch - old->pitch) / (sysTime - old->time);

            old->yaw   = yaw;
            old->pitch = smoothView.pitch;
            old->time  = sysTime;
        }

        // The Rdx and Rdy should stay constant when moving.
        if(showViewPosDeltas)
        {
            struct OldPos {
                ddouble time;
                Vector3f pos;
            };

            static OldPos oldPos[DDMAXPLAYERS];
            OldPos *old = &oldPos[viewPlayer - ddPlayers];

            LOGDEV_MSG("(%i) F=%.3f dt=%-10.3f dx=%-10.3f dy=%-10.3f dz=%-10.3f dx/dt=%-10.3f dy/dt=%-10.3f")
                    << SECONDS_TO_TICKS(gameTime)
                    << frameTimePos
                    << sysTime - old->time
                    << smoothView.origin.x - old->pos.x
                    << smoothView.origin.y - old->pos.y
                    << smoothView.origin.z - old->pos.z
                    << (smoothView.origin.x - old->pos.x) / (sysTime - old->time)
                    << (smoothView.origin.y - old->pos.y) / (sysTime - old->time);

            old->pos  = smoothView.origin;
            old->time = sysTime;
        }
    }

    // Update viewer.
    angle_t const viewYaw = vd->current.angle();

    duint const an = viewYaw >> ANGLETOFINESHIFT;
    vd->viewSin = FIX2FLT(finesine[an]);
    vd->viewCos = FIX2FLT(fineCosine[an]);

    // Calculate the front, up and side unit vectors.
    dfloat const yawRad   = ((viewYaw / (dfloat) ANGLE_MAX) *2) * PI;
    dfloat const pitchRad = vd->current.pitch * 85 / 110.f / 180 * PI;

    // The front vector.
    vd->frontVec.x = cos(yawRad) * cos(pitchRad);
    vd->frontVec.z = sin(yawRad) * cos(pitchRad);
    vd->frontVec.y = sin(pitchRad);

    // The up vector.
    vd->upVec.x = -cos(yawRad) * sin(pitchRad);
    vd->upVec.z = -sin(yawRad) * sin(pitchRad);
    vd->upVec.y = cos(pitchRad);

    // The side vector is the cross product of the front and up vectors.
    vd->sideVec = vd->frontVec.cross(vd->upVec);
}

/**
 * Prepare rendering the view of the given player.
 */
void R_SetupFrame(player_t *player)
{
#define MINEXTRALIGHTFRAMES         2

    // This is now the current view player.
    viewPlayer = player;

    // Reset the GL triangle counter.
    //polyCounter = 0;

    if(showFrameTimePos)
    {
        LOGDEV_VERBOSE("frametime = %f") << frameTimePos;
    }

    // Handle extralight (used to light up the world momentarily (used for
    // e.g. gun flashes). We want to avoid flickering, so when ever it is
    // enabled; make it last for a few frames.
    if(player->targetExtraLight != player->shared.extraLight)
    {
        player->targetExtraLight = player->shared.extraLight;
        player->extraLightCounter = MINEXTRALIGHTFRAMES;
    }

    if(player->extraLightCounter > 0)
    {
        player->extraLightCounter--;
        if(player->extraLightCounter == 0)
            player->extraLight = player->targetExtraLight;
    }

    // Why?
    validCount++;

    extraLight      = player->extraLight;
    extraLightDelta = extraLight / 16.0f;

    if(!freezeRLs)
    {
        R_ClearVisSprites();
    }

#undef MINEXTRALIGHTFRAMES
}

void R_RenderPlayerViewBorder()
{
    R_DrawViewBorder();
}

void R_UseViewPort(viewport_t const *vp)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    if(!vp)
    {
        currentViewport = nullptr;
        ClientWindow::main().game().glApplyViewport(
                Rectanglei::fromSize(Vector2i(DENG_GAMEVIEW_X, DENG_GAMEVIEW_Y),
                                     Vector2ui(DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT)));
    }
    else
    {
        currentViewport = const_cast<viewport_t *>(vp);
        ClientWindow::main().game().glApplyViewport(vp->geometry);
    }
}

viewport_t const *R_CurrentViewPort()
{
    return currentViewport;
}

void R_RenderBlankView()
{
    UI_DrawDDBackground(Point2Raw(0, 0), Size2Raw(320, 200), 1);
}

static void setupPlayerSprites()
{
    psp3d = false;

    // Cameramen have no psprites.
    ddplayer_t *ddpl = &viewPlayer->shared;
    if((ddpl->flags & DDPF_CAMERA) || (ddpl->flags & DDPF_CHASECAM))
        return;

    if(!ddpl->mo) return;
    mobj_t *mob = ddpl->mo;

    if(!Mobj_HasSubspace(*mob)) return;
    SectorCluster &cluster = Mobj_Cluster(*mob);

    // Determine if we should be drawing all the psprites full bright?
    dd_bool isFullBright = (levelFullBright != 0);
    if(!isFullBright)
    {
        ddpsprite_t *psp = ddpl->pSprites;
        for(dint i = 0; i < DDMAXPSPRITES; ++i, psp++)
        {
            if(!psp->statePtr) continue;

            // If one of the psprites is fullbright, both are.
            if(psp->statePtr->flags & STF_FULLBRIGHT)
                isFullBright = true;
        }
    }

    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);

    ddpsprite_t *psp = ddpl->pSprites;
    for(dint i = 0; i < DDMAXPSPRITES; ++i, psp++)
    {
        vispsprite_t *spr = &visPSprites[i];

        spr->type = VPSPR_SPRITE;
        spr->psp  = psp;

        if(!psp->statePtr) continue;

        // First, determine whether this is a model or a sprite.
        bool isModel = false;
        ModelDef *mf = nullptr, *nextmf = nullptr;
        dfloat inter = 0;
        if(useModels)
        {
            // Is there a model for this frame?
            MobjThinker dummy;

            // Setup a dummy for the call to R_CheckModelFor.
            dummy->state = psp->statePtr;
            dummy->tics = psp->tics;

            mf = Mobj_ModelDef(dummy, &nextmf, &inter);
            if(mf) isModel = true;
        }

        if(isModel)
        {
            // Yes, draw a 3D model (in Rend_Draw3DPlayerSprites).
            // There are 3D psprites.
            psp3d = true;

            spr->type   = VPSPR_MODEL;
            spr->origin = viewData->current.origin;

            spr->data.model.bspLeaf     = &Mobj_BspLeafAtOrigin(*mob);
            spr->data.model.flags       = 0;
            // 32 is the raised weapon height.
            spr->data.model.topZ        = viewData->current.origin.z;
            spr->data.model.secFloor    = cluster.visFloor().heightSmoothed();
            spr->data.model.secCeil     = cluster.visCeiling().heightSmoothed();
            spr->data.model.pClass      = 0;
            spr->data.model.floorClip   = 0;

            spr->data.model.mf          = mf;
            spr->data.model.nextMF      = nextmf;
            spr->data.model.inter       = inter;
            spr->data.model.viewAligned = true;

            // Offsets to rotation angles.
            spr->data.model.yawAngleOffset   = psp->pos[0] * weaponOffsetScale - 90;
            spr->data.model.pitchAngleOffset =
                (32 - psp->pos[1]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
            // Is the FOV shift in effect?
            if(weaponFOVShift > 0 && Rend_FieldOfView() > 90)
                spr->data.model.pitchAngleOffset -= weaponFOVShift * (Rend_FieldOfView() - 90) / 90;
            // Real rotation angles.
            spr->data.model.yaw =
                viewData->current.angle() / (dfloat) ANGLE_MAX *-360 + spr->data.model.yawAngleOffset + 90;
            spr->data.model.pitch = viewData->current.pitch * 85 / 110 + spr->data.model.yawAngleOffset;
            std::memset(spr->data.model.visOff, 0, sizeof(spr->data.model.visOff));

            spr->data.model.alpha = psp->alpha;
            spr->data.model.stateFullBright = (psp->flags & DDPSPF_FULLBRIGHT)!=0;
        }
        else
        {
            // No, draw a 2D sprite (in Rend_DrawPlayerSprites).
            spr->type = VPSPR_SPRITE;

            // Adjust the center slightly so an angle can be calculated.
            spr->origin = viewData->current.origin;

            spr->data.sprite.bspLeaf      = &Mobj_BspLeafAtOrigin(*mob);
            spr->data.sprite.alpha        = psp->alpha;
            spr->data.sprite.isFullBright = (psp->flags & DDPSPF_FULLBRIGHT) != 0;
        }
    }
}

static Matrix4f frameViewMatrix;

static void setupViewMatrix()
{
    // This will be the view matrix for the current frame.
    frameViewMatrix = GL_GetProjectionMatrix() *
                      Rend_GetModelViewMatrix(viewPlayer - ddPlayers);
}

Matrix4f const &Viewer_Matrix()
{
    return frameViewMatrix;
}

#undef R_RenderPlayerView
DENG_EXTERN_C void R_RenderPlayerView(dint num)
{
    if(num < 0 || num >= DDMAXPLAYERS) return; // Huh?
    player_t *player = &ddPlayers[num];

    if(!player->shared.inGame) return;
    if(!player->shared.mo) return;

    if(firstFrameAfterLoad)
    {
        // Don't let the clock run yet.  There may be some texture
        // loading still left to do that we have been unable to
        // predetermine.
        firstFrameAfterLoad = false;
        DD_ResetTimer();
    }

    // Too early? Game has not configured the view window?
    viewdata_t *vd = &viewDataOfConsole[num];
    if(vd->window.isNull()) return;

    // Setup for rendering the frame.
    R_SetupFrame(player);

    vrCfg().setEyeHeightInMapUnits(Con_GetInteger("player-eyeheight"));

    setupViewMatrix();
    setupPlayerSprites();

    if(ClientApp::vr().mode() == VRConfig::OculusRift &&
       worldSys().isPointInVoid(Rend_EyeOrigin().xzy()))
    {
        // Putting one's head in the wall will cause a blank screen.
        GLState::current().target().clear(GLTarget::Color);
        return;
    }

    // Hide the viewPlayer's mobj?
    dint oldFlags = 0;
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        oldFlags = player->shared.mo->ddFlags;
        player->shared.mo->ddFlags |= DDMF_DONTDRAW;
    }

    // Go to wireframe mode?
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // GL is in 3D transformation state only during the frame.
    GL_SwitchTo3DState(true, currentViewport, vd);

    if(worldSys().hasMap())
    {
        Rend_RenderMap(worldSys().map());
    }

    // Orthogonal projection to the view window.
    GL_Restore2DState(1, currentViewport, vd);

    // Don't render in wireframe mode with 2D psprites.
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    Rend_Draw2DPlayerSprites();  // If the 2D versions are needed.

    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    // Do we need to render any 3D psprites?
    if(psp3d)
    {
        GL_SwitchTo3DState(false, currentViewport, vd);
        Rend_Draw3DPlayerSprites();
    }

    // Restore fullscreen viewport, original matrices and state: back to normal 2D.
    GL_Restore2DState(2, currentViewport, vd);

    // Back from wireframe mode?
    if(renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Now we can show the viewPlayer's mobj again.
    if(!(player->shared.flags & DDPF_CHASECAM))
    {
        player->shared.mo->ddFlags = oldFlags;
    }

    R_PrintRendPoolInfo();

#ifdef LIBDENG_CAMERA_MOVEMENT_ANALYSIS
    {
        static dfloat prevPos[3] = { 0, 0, 0 };
        static dfloat prevSpeed = 0;
        static dfloat prevTime;
        dfloat delta[2] = { vd->current.pos[0] - prevPos[0],
                            vd->current.pos[1] - prevPos[1] };
        dfloat speed   = V2f_Length(delta);
        dfloat time    = sysTime - devCameraMovementStartTime;
        dfloat elapsed = time - prevTime;

        LOGDEV_MSG("%f,%f,%f,%f,%f") << Sys_GetRealSeconds() - devCameraMovementStartTimeRealSecs
                                     << time << elapsed << speed/elapsed << speed/elapsed - prevSpeed;

        V3f_Copy(prevPos, vd->current.pos);
        prevSpeed = speed/elapsed;
        prevTime = time;
    }
#endif
}

/**
 * Should be called when returning from a game-side drawing method to ensure
 * that our assumptions of the GL state are valid. This is necessary because
 * DGL affords the user the posibility of modifiying the GL state.
 *
 * @todo: A cleaner approach would be a DGL state stack which could simply pop.
 */
static void restoreDefaultGLState()
{
    // Here we use the DGL methods as this ensures it's state is kept in sync.
    DGL_Disable(DGL_FOG);
    DGL_Disable(DGL_SCISSOR_TEST);
    DGL_Disable(DGL_TEXTURE_2D);
    DGL_Enable(DGL_LINE_SMOOTH);
    DGL_Enable(DGL_POINT_SMOOTH);
}

static void clearViewPorts()
{
    GLbitfield bits = GL_DEPTH_BUFFER_BIT;

    if(fx::Bloom::isEnabled() ||
       (App_InFineSystem().finaleInProgess() && !GameUIWidget::finaleStretch()) ||
       ClientApp::vr().mode() == VRConfig::OculusRift)
    {
        // Parts of the previous frame might leak in the bloom unless we clear the color
        // buffer. Not doing this would result in very bright HOMs in map holes and game
        // UI elements glowing in the frame (UI elements are normally on a separate layer
        // and should not affect bloom).
        bits |= GL_COLOR_BUFFER_BIT;
    }

    if(!devRendSkyMode)
        bits |= GL_STENCIL_BUFFER_BIT;

    if(freezeRLs)
    {
        bits |= GL_COLOR_BUFFER_BIT;
    }
    else
    {
        for(dint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];

            if(!plr->shared.inGame || !(plr->shared.flags & DDPF_LOCAL))
                continue;

            if(P_IsInVoid(plr) || !worldSys().hasMap())
            {
                bits |= GL_COLOR_BUFFER_BIT;
                break;
            }
        }
    }

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    // This is all the clearing we'll do.
    glClear(bits);
}

void R_RenderViewPorts(ViewPortLayer layer)
{
    dint oldDisplay = displayPlayer;

    // First clear the viewport.
    if(layer == Player3DViewLayer)
    {
        clearViewPorts();
    }

    // Draw a view for all players with a visible viewport.
    for(dint p = 0, y = 0; y < gridRows; ++y)
    for(dint x = 0; x < gridCols; x++, ++p)
    {
        viewport_t const *vp = &viewportOfLocalPlayer[p];

        displayPlayer = vp->console;
        R_UseViewPort(vp);

        if(displayPlayer < 0 || (ddPlayers[displayPlayer].shared.flags & DDPF_UNDEFINED_ORIGIN))
        {
            if(layer == Player3DViewLayer)
            {
                R_RenderBlankView();
            }
            continue;
        }

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();

        // Use an orthographic projection in real pixel dimensions.
        glOrtho(0, vp->geometry.width(), vp->geometry.height(), 0, -1, 1);

        viewdata_t const *vd = &viewDataOfConsole[vp->console];
        RectRaw vpGeometry(vp->geometry.topLeft.x, vp->geometry.topLeft.y,
                           vp->geometry.width(), vp->geometry.height());

        RectRaw vdWindow(vd->window.topLeft.x, vd->window.topLeft.y,
                         vd->window.width(), vd->window.height());

        switch(layer)
        {
        case Player3DViewLayer:
            R_UpdateViewer(vp->console);
            LensFx_BeginFrame(vp->console);
            gx.DrawViewPort(p, &vpGeometry, &vdWindow, displayPlayer, 0/*layer #0*/);
            LensFx_EndFrame();
            break;

        case ViewBorderLayer:
            R_RenderPlayerViewBorder();
            break;

        case HUDLayer:
            gx.DrawViewPort(p, &vpGeometry, &vdWindow, displayPlayer, 1/*layer #1*/);
            break;
        }

        restoreDefaultGLState();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }

    if(layer == Player3DViewLayer)
    {
        // Increment the internal frame count. This does not
        // affect the window's FPS counter.
        frameCount++;

        // Keep reseting until a new sharp world has arrived.
        if(resetNextViewer > 1)
            resetNextViewer = 0;
    }

    // Restore things back to normal.
    displayPlayer = oldDisplay;
    R_UseViewPort(nullptr);
}

void R_ClearViewData()
{
    M_Free(luminousDist); luminousDist = nullptr;
    M_Free(luminousClipped); luminousClipped = nullptr;
    M_Free(luminousOrder); luminousOrder = nullptr;
}

/**
 * Viewer specific override controlling whether a given sky layer is enabled.
 *
 * @todo The override should be applied at SkyDrawable level. We have Raven to
 * thank for this nonsense (Hexen's sector special 200)... -ds
 */
#undef R_SkyParams
DENG_EXTERN_C void R_SkyParams(dint layerIndex, dint param, void * /*data*/)
{
    LOG_AS("R_SkyParams");
    if(!worldSys().hasMap())
    {
        LOG_GL_WARNING("No map currently loaded, ignoring");
        return;
    }

    Sky &sky = worldSys().map().sky();
    if(layerIndex >= 0 && layerIndex < sky.layerCount())
    {
        SkyLayer *layer = sky.layer(layerIndex);
        switch(param)
        {
        case DD_ENABLE:  layer->enable();  break;
        case DD_DISABLE: layer->disable(); break;

        default:
            // Log but otherwise ignore this error.
            LOG_GL_WARNING("Failed configuring layer #%i: bad parameter %i")
                    << layerIndex << param;
        }
        return;
    }
    LOG_GL_WARNING("Invalid layer #%i") << + layerIndex;
}

bool R_ViewerSubspaceIsVisible(ConvexSubspace const &subspace)
{
    DENG2_ASSERT(subspace.indexInMap() != MapElement::NoIndex);
    return subspacesVisible.testBit(subspace.indexInMap());
}

void R_ViewerSubspaceMarkVisible(ConvexSubspace const &subspace, bool yes)
{
    DENG2_ASSERT(subspace.indexInMap() != MapElement::NoIndex);
    subspacesVisible.setBit(subspace.indexInMap(), yes);
}

bool R_ViewerGeneratorIsVisible(Generator const &generator)
{
    return generatorsVisible.testBit(generator.id() - 1 /* id is 1-based index */);
}

void R_ViewerGeneratorMarkVisible(Generator const &generator, bool yes)
{
    generatorsVisible.setBit(generator.id() - 1 /* id is 1-based index */, yes);
}

ddouble R_ViewerLumobjDistance(dint idx)
{
    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < worldSys().map().lumobjCount())
    {
        return luminousDist[idx];
    }
    return 0;
}

bool R_ViewerLumobjIsClipped(dint idx)
{
    // If we are not yet prepared for this, just say everything is clipped.
    if(!luminousClipped) return true;

    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < worldSys().map().lumobjCount())
    {
        return CPP_BOOL(luminousClipped[idx]);
    }
    return false;
}

bool R_ViewerLumobjIsHidden(dint idx)
{
    // If we are not yet prepared for this, just say everything is hidden.
    if(!luminousClipped) return true;

    /// @todo Do not assume the current map.
    if(idx >= 0 && idx < worldSys().map().lumobjCount())
    {
        return luminousClipped[idx] == 2;
    }
    return false;
}

static void markLumobjClipped(Lumobj const &lob, bool yes = true)
{
    dint const index = lob.indexInMap();
    DENG2_ASSERT(index >= 0 && index < lob.map().lumobjCount());
    luminousClipped[index] = yes? 1 : 0;
}

/// Used to sort lumobjs by distance from viewpoint.
static dint lumobjSorter(void const *e1, void const *e2)
{
    coord_t a = luminousDist[*(duint const *) e1];
    coord_t b = luminousDist[*(duint const *) e2];
    if(a > b) return 1;
    if(a < b) return -1;
    return 0;
}

void R_BeginFrame()
{
    Map &map = worldSys().map();

    subspacesVisible.resize(map.subspaceCount());
    subspacesVisible.fill(false);

    // Clear all generator visibility flags.
    generatorsVisible.fill(false);

    dint numLuminous = map.lumobjCount();
    if(!(numLuminous > 0)) return;

    // Resize the associated buffers used for per-frame stuff.
    dint maxLuminous = numLuminous;
    luminousDist    = (coord_t *)  M_Realloc(luminousDist,    sizeof(*luminousDist)    * maxLuminous);
    luminousClipped =    (dbyte *) M_Realloc(luminousClipped, sizeof(*luminousClipped) * maxLuminous);
    luminousOrder   =    (duint *) M_Realloc(luminousOrder,   sizeof(*luminousOrder)   * maxLuminous);

    // Update viewer => lumobj distances ready for linking and sorting.
    viewdata_t const *viewData = R_ViewData(viewPlayer - ddPlayers);
    map.forAllLumobjs([&viewData] (Lumobj &lob)
    {
        // Approximate the distance in 3D.
        Vector3d delta = lob.origin() - viewData->current.origin;
        luminousDist[lob.indexInMap()] = M_ApproxDistance3(delta.x, delta.y, delta.z * 1.2 /*correct aspect*/);
        return LoopContinue;
    });

    if(rendMaxLumobjs > 0 && numLuminous > rendMaxLumobjs)
    {
        // Sort lumobjs by distance from the viewer. Then clip all lumobjs
        // so that only the closest are visible (max loMaxLumobjs).

        // Init the lumobj indices, sort array.
        for(dint i = 0; i < numLuminous; ++i)
        {
            luminousOrder[i] = i;
        }
        qsort(luminousOrder, numLuminous, sizeof(duint), lumobjSorter);

        // Mark all as hidden.
        std::memset(luminousClipped, 2, numLuminous * sizeof(*luminousClipped));

        dint n = 0;
        for(dint i = 0; i < numLuminous; ++i)
        {
            if(n++ > rendMaxLumobjs)
                break;

            // Unhide this lumobj.
            luminousClipped[luminousOrder[i]] = 1;
        }
    }
    else
    {
        // Mark all as clipped.
        std::memset(luminousClipped, 1, numLuminous * sizeof(*luminousClipped));
    }
}

void R_ViewerClipLumobj(Lumobj *lum)
{
    if(!lum) return;

    // Has this already been occluded?
    dint lumIdx = lum->indexInMap();
    if(luminousClipped[lumIdx] > 1)
        return;

    markLumobjClipped(*lum, false);

    /// @todo Determine the exact centerpoint of the light in addLuminous!
    Vector3d const origin(lum->x(), lum->y(), lum->z() + lum->zOffset());

    if(!(devNoCulling || P_IsInVoid(&ddPlayers[displayPlayer])))
    {
        if(!rendSys().angleClipper().isPointVisible(origin))
        {
            markLumobjClipped(*lum); // Won't have a halo.
        }
    }
    else
    {
        markLumobjClipped(*lum);

        Vector3d const eye = Rend_EyeOrigin().xzy();
        if(LineSightTest(eye, origin, -1, 1, LS_PASSLEFT | LS_PASSOVER | LS_PASSUNDER)
                .trace(lum->map().bspTree()))
        {
            markLumobjClipped(*lum, false); // Will have a halo.
        }
    }
}

void R_ViewerClipLumobjBySight(Lumobj *lob, ConvexSubspace *subspace)
{
    if(!lob || !subspace) return;

    // Already clipped?
    if(luminousClipped[lob->indexInMap()])
        return;

    // We need to figure out if any of the polyobj's segments lies
    // between the viewpoint and the lumobj.
    Vector3d const eye = Rend_EyeOrigin().xzy();

    subspace->forAllPolyobjs([&lob, &eye] (Polyobj &pob)
    {
        for(HEdge *hedge : pob.mesh().hedges())
        {
            // Is this on the back of a one-sided line?
            if(!hedge->hasMapElement())
                continue;

            // Ignore half-edges facing the wrong way.
            if(hedge->mapElementAs<LineSideSegment>().isFrontFacing())
            {
                coord_t eyeV1[2]       = { eye.x, eye.y };
                coord_t lumOriginV1[2] = { lob->origin().x, lob->origin().y };
                coord_t fromV1[2]      = { hedge->origin().x, hedge->origin().y };
                coord_t toV1[2]        = { hedge->twin().origin().x, hedge->twin().origin().y };
                if(V2d_Intercept2(lumOriginV1, eyeV1, fromV1, toV1, 0, 0, 0))
                {
                    markLumobjClipped(*lob);
                    break;
                }
            }
        }
        return LoopContinue;
    });
}

angle_t viewer_t::angle() const
{
    angle_t a = _angle;
    if(DD_GetInteger(DD_USING_HEAD_TRACKING))
    {
        // Apply the actual, current yaw offset. The game has omitted the "body yaw"
        // portion from the value already.
        a += fixed_t(radianToDegree(vrCfg().oculusRift().headOrientation().z) / 180 * ANGLE_180);
    }
    return a;
}

D_CMD(ViewGrid)
{
    DENG2_UNUSED2(src, argc);
    // Recalculate viewports.
    return R_SetViewGrid(String(argv[1]).toInt(), String(argv[2]).toInt());
}

void Viewports_Register()
{
    C_VAR_INT ("con-show-during-setup",     &loadInStartupMode,     0, 0, 1);

    C_VAR_INT ("rend-camera-smooth",        &rendCameraSmooth,      CVF_HIDE, 0, 1);

    C_VAR_BYTE("rend-info-deltas-angles",   &showViewAngleDeltas,   0, 0, 1);
    C_VAR_BYTE("rend-info-deltas-pos",      &showViewPosDeltas,     0, 0, 1);
    C_VAR_BYTE("rend-info-frametime",       &showFrameTimePos,      0, 0, 1);
    C_VAR_BYTE("rend-info-rendpolys",       &rendInfoRPolys,        CVF_NO_ARCHIVE, 0, 1);
    //C_VAR_INT ("rend-info-tris",            &rendInfoTris,          0, 0, 1); // not implemented atm

    C_CMD("viewgrid", "ii", ViewGrid);
}
