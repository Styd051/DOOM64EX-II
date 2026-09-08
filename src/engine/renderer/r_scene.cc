// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007-2012 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "gl_main.h"
#include "gl_texture.h"
#include "r_local.h"
#include "r_sky.h"
#include "r_drawlist.h"
#include "shader/draw.hh"

extern cvar::BoolVar i_interpolateframes;
extern cvar::BoolVar r_texturecombiner;
extern cvar::BoolVar r_fog;
extern cvar::BoolVar r_rendersprites;
extern cvar::BoolVar st_flashoverlay;
extern cvar::BoolVar r_colorizesubsectors;
extern cvar::BoolVar r_depthbuffer;

//
// ProcessWalls
//

static dboolean ProcessWalls(vtxlist_t* vl, int* drawcount) {
    seg_t* seg = (seg_t*)vl->data;
    sector_t* sec = seg->frontsector;

    bspColor[LIGHT_FLOOR]    = R_GetSectorLight(0xff, sec->colors[LIGHT_FLOOR]);
    bspColor[LIGHT_CEILING] = R_GetSectorLight(0xff, sec->colors[LIGHT_CEILING]);
    bspColor[LIGHT_THING]    = R_GetSectorLight(0xff, sec->colors[LIGHT_THING]);
    bspColor[LIGHT_UPRWALL] = R_GetSectorLight(0xff, sec->colors[LIGHT_UPRWALL]);
    bspColor[LIGHT_LWRWALL] = R_GetSectorLight(0xff, sec->colors[LIGHT_LWRWALL]);

    // [kex] r_ColorizeSubsectors. Every one of the five, or the seam between
    // two subsectors would still be readable as a light change rather than as
    // the boundary it is.
    if(r_colorizesubsectors) {
        rcolor c = R_SubsectorColor(vl->subsector);
        int i;

        for(i = 0; i < 5; i++) {
            bspColor[i] = c;
        }
    }

    if(!vl->callback(seg, &drawVertex[*drawcount])) {
        return false;
    }

    dglTriangle(*drawcount + 0, *drawcount + 1, *drawcount + 2);
    dglTriangle(*drawcount + 3, *drawcount + 2, *drawcount + 1);

    *drawcount += 4;

    return true;
}

//
// ProcessFlats
//

static dboolean ProcessFlats(vtxlist_t* vl, int* drawcount) {
    int j;
    fixed_t tx;
    fixed_t ty;
    leaf_t* leaf;
    subsector_t* ss;
    sector_t* sector;
    int count;

    ss      = (subsector_t*)vl->data;
    leaf    = &leafs[ss->leaf];
    sector  = ss->sector;
    count   = *drawcount;

    for(j = 0; j < ss->numleafs - 2; j++) {
        dglTriangle(count, count + 1 + j, count + 2 + j);
    }

    // need to keep texture coords small to avoid
    // floor 'wobble' due to rounding errors on some cards
    // make relative to first vertex, not (0,0)
    // which is arbitary anyway

    tx = (leaf->vertex->x >> 6) & ~(FRACUNIT - 1);
    ty = (leaf->vertex->y >> 6) & ~(FRACUNIT - 1);

    for(j = 0; j < ss->numleafs; j++) {
        int idx;
        vtx_t *v = &drawVertex[count];

        if(vl->flags & DLF_CEILING) {
            leaf = &leafs[(ss->leaf + (ss->numleafs - 1)) - j];
        }
        else {
            leaf = &leafs[ss->leaf + j];
        }

        v->x = F2D3D(leaf->vertex->x);
        v->y = F2D3D(leaf->vertex->y);

        if(vl->flags & DLF_CEILING) {
            if(i_interpolateframes) {
                v->z = F2D3D(sector->frame_z2[1]);
            } else {
                v->z = F2D3D(sector->ceilingheight);
            }
        }
        else {
            if(i_interpolateframes) {
                v->z = F2D3D(sector->frame_z1[1]);
            }
            else {
                v->z = F2D3D(sector->floorheight);
            }
        }

        v->tu = F2D3D((leaf->vertex->x >> 6) - tx);
        v->tv = -F2D3D((leaf->vertex->y >> 6) - ty);

        // set the mapping offsets for scrolling floors/ceilings
        if((!(vl->flags & DLF_CEILING) && sector->flags & MS_SCROLLFLOOR) ||
                (vl->flags & DLF_CEILING && sector->flags & MS_SCROLLCEILING)) {
            v->tu   += F2D3D(sector->xoffset >> 6);
            v->tv   += F2D3D(sector->yoffset >> 6);
        }

        v->a = 0xff;

        if(vl->flags & DLF_CEILING) {
            idx = sector->colors[LIGHT_CEILING];
        }
        else {
            idx = sector->colors[LIGHT_FLOOR];
        }

        R_LightToVertex(v, idx, 1);

        if(r_colorizesubsectors) {
            rcolor c = R_SubsectorColor(vl->subsector);

            v->r = (byte)((c >> 0) & 0xff);
            v->g = (byte)((c >> 8) & 0xff);
            v->b = (byte)((c >> 16) & 0xff);
        }

        //
        // water layer 1
        //
        if(vl->flags & DLF_WATER1) {
            v->tv -= F2D3D(scrollfrac >> 6);
            v->a = 0xA0;
        }

        //
        // water layer 2
        //
        if(vl->flags & DLF_WATER2) {
            v->tu += F2D3D(scrollfrac >> 6);
        }

        count++;
    }

    *drawcount = count;

    return true;
}

//
// ProcessSprites
//

static dboolean ProcessSprites(vtxlist_t* vl, int* drawcount) {
    visspritelist_t* vis;
    mobj_t* mobj;

    vis = (visspritelist_t*)vl->data;
    mobj = vis->spr;

    if(!mobj) {
        return false;
    }

    if(!vl->callback(vis, &drawVertex[*drawcount])) {
        return false;
    }

    GL_SetState(GLSTATE_CULL, !(mobj->flags & MF_RENDERLASER));

    dglTriangle(*drawcount + 0, *drawcount + 1, *drawcount + 2);
    dglTriangle(*drawcount + 3, *drawcount + 2, *drawcount + 1);

    *drawcount += 4;

    return true;
}

//
// SetupFog
//
// Sky flats determine how fog is rendered. this includes
// fog color, distance and density. The factor for fog
// density is based on values from the original N64 version.
//

static void SetupFog(void) {
    dglFogi(GL_FOG_MODE, GL_LINEAR);

    // don't render fog in wireframe mode
    if(!r_fillmode) {
        return;
    }

    if(!skyflatnum) {
        dglDisable(GL_FOG);
    }
    else if(r_fog) {
        rfloat color[4] = { 0, 0, 0, 0 };
        rcolor fogcolor = 0;
        int fognear = 0;
        int fogfactor;

        // density factors range from 990 to 900
        // each step in density is suppose to be (128 * 1000)
        // 985 is the default if no sky is present at all

        fognear = sky ? sky->fognear : 985;
        fogfactor = (1000 - fognear);

        if(fogfactor <= 0) {
            fogfactor = 1;
        }

        dglEnable(GL_FOG);

        //
        // Tell the programmable path where the fog runs, in world units, from
        // the original's own numbers rather than from the exponential below.
        //
        // fognear is the N64's FOG_MIN: a position in the normalised screen
        // depth range, in thousandths, with the fog complete at the far plane.
        // The original's projection is guFrustum(-8, 8, -6, 6, 8, 3808)
        // (DOOM64-RE, r_main.c:113), so inverting its perspective divide turns a
        // screen depth back into a world distance:
        //
        //     screen_z(w) = far / (far - near) * (1 - near / w)
        //     w           = near / (1 - s * (far - near) / far)
        //
        // The trap is that the original ramps linearly in *screen* depth, and
        // the perspective divide makes that violently front-loaded in world
        // terms. At fognear 975 the N64 is already at 44% fog by 500 units and
        // 76% by 1000, while its endpoints are 296 and 3808. Feeding those
        // endpoints to doomSceneMain -- which smoothsteps in linear world
        // distance -- gives 1% and 10%: almost no fog at all. Tried, and
        // visibly wrong.
        //
        // A symmetric smoothstep cannot follow that curve, so the endpoints are
        // not what to match. What follows is fitted to the curve itself, over
        // the first 2500 units, and the fit is far better than either the
        // endpoints or the exponential below:
        //
        //     endpoints (296 .. 3808)   worse than doing nothing
        //     Doom64EX's own (0 .. 2139)   RMS error 0.197
        //     this, (0 .. 2.4 x w50)       RMS error 0.096
        //
        // where w50 is the distance at which the original reaches half fog.
        // Starting at zero costs about 6% fog on surfaces right in front of the
        // player, which the original does not have; that is the price of a
        // symmetric curve, and it buys the whole mid-range where the fog is
        // actually seen.
        //
        {
            const float n64_near = 8.0f;
            const float n64_far = 3808.0f;

            // Screen depth halfway between where fog starts and where it is
            // complete, then back to a world distance.
            float s50 = (((float)fognear / 1000.0f) + 1.0f) * 0.5f;
            float denom = 1.0f - s50 * ((n64_far - n64_near) / n64_far);

            if(denom < 0.0001f) {
                denom = 0.0001f;
            }

            imp::shader::set_world_fog(0.0f, 2.4f * (n64_near / denom));
        }

        // do exponential fog if color is black
        if(sky && (sky->fogcolor & 0xFFFFFF) != 0) {
            int min;
            int max;

            max = 128000 / fogfactor;
            min = ((fognear - 500) * 256) / fogfactor;

            fogcolor = sky->fogcolor;
            dglFogi(GL_FOG_MODE, GL_EXP);
            dglFogf(GL_FOG_DENSITY, 14.0f / (max + min));
        }
        // do linear rendering for colored fog
        else {
            float min;
            float max;
            float position;

            position = ((float)fogfactor / 1000.0f);

            if(position <= 0.0f) {
                position = 0.00001f;
            }

            min = 5.0f / position;
            max = 30.0f / position;

            dglFogf(GL_FOG_START, min);
            dglFogf(GL_FOG_END, max);
        }

        dglGetColorf(fogcolor, color);
        dglFogfv(GL_FOG_COLOR, color);
    }
}

//
// R_SetViewMatrix
//

void R_SetViewMatrix(void) {
    dglMatrixMode(GL_PROJECTION);
    dglLoadIdentity();
    dglViewFrustum(video_width, video_height, *r_fov, 0.1f);
    dglMatrixMode(GL_MODELVIEW);
    dglLoadIdentity();
    dglRotatef(-TRUEANGLES(viewpitch), 1.0f, 0.0f, 0.0f);
    dglRotatef(-TRUEANGLES(viewangle) + 90.0f, 0.0f, 0.0f, 1.0f);
    dglTranslatef(-fviewx, -fviewy, -fviewz);
}

//
// R_RenderWorld
//

void R_RenderWorld(void) {
    dboolean painter = !*r_depthbuffer;

    SetupFog();

    //
    // The painter path leaves the depth buffer alone from beginning to end.
    // Submission order is the whole answer there: the walk hands out the
    // farthest subsector first, so anything drawn later is in front by
    // construction and has every right to cover what came before.
    //
    if(!painter) {
        dglEnable(GL_DEPTH_TEST);
    }

    DL_BeginDrawList(*r_fillmode, *r_texturecombiner);

    // setup texture environment for effects
    if(r_texturecombiner) {
        if(!nolights) {
            GL_UpdateEnvTexture(WHITE);
            GL_SetTextureUnit(1, true);
            dglTexCombModulate(GL_PREVIOUS, GL_PRIMARY_COLOR);
        }

        if(!st_flashoverlay) {
            GL_SetTextureUnit(2, true);
            dglTexCombColor(GL_PREVIOUS, flashcolor, GL_ADD);
        }

        dglTexCombReplaceAlpha(GL_TEXTURE0_ARB);

        GL_SetTextureUnit(0, true);
    }
    else {
        GL_SetTextureUnit(1, true);
        GL_SetTextureMode(GL_ADD);
        GL_SetTextureUnit(0, true);

        if(nolights) {
            GL_SetTextureMode(GL_REPLACE);
        }
    }

    dglEnable(GL_ALPHA_TEST);

    // begin draw list loop

    if(painter) {
        //
        // Sprites have to become drawlist entries before anything is drawn,
        // because on this path they are interleaved with the geometry rather
        // than following it. R_SetupSprites also settles which subsector each
        // one belongs to, which needs the whole walk to have finished.
        //
        if(devparm) {
            spriteRenderTic = I_GetTimeMS();
        }

        if(r_rendersprites) {
            R_SetupSprites();
        }

        // Blending is switched per class inside, since walls and flats now
        // alternate once per subsector instead of being two whole passes.
        DL_ProcessWorldPainter(ProcessWalls, ProcessFlats, ProcessSprites);
    }
    else {
        // -------------- Draw walls (segs) --------------------------

        DL_ProcessDrawList(DLT_WALL, ProcessWalls);

        // -------------- Draw floors/ceilings (leafs) ---------------

        GL_SetState(GLSTATE_BLEND, 1);
        DL_ProcessDrawList(DLT_FLAT, ProcessFlats);

        // -------------- Draw things (sprites) ----------------------

        if(devparm) {
            spriteRenderTic = I_GetTimeMS();
        }

        if(r_rendersprites) {
            R_SetupSprites();
        }

        dglDepthMask(GL_FALSE);
        DL_ProcessDrawList(DLT_SPRITE, ProcessSprites);
    }

    // -------------- Restore states -----------------------------

    dglDisable(GL_ALPHA_TEST);
    dglDepthMask(GL_TRUE);
    dglDisable(GL_FOG);
    dglDisable(GL_DEPTH_TEST);

    GL_SetOrthoScale(1.0f);
    GL_SetState(GLSTATE_BLEND, 0);
    GL_SetState(GLSTATE_CULL, 1);
    GL_SetDefaultCombiner();

    // villsa 12152013 - make sure we're using the default blend function
    dglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

