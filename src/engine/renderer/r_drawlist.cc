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
// DESCRIPTION: Vertex draw lists.
// Stores geometry info produced by R_RenderBSPNode into a list for optimal rendering
//
//-----------------------------------------------------------------------------

#include "doomdef.h"
#include "doomstat.h"
#include "d_devstat.h"
#include "r_local.h"
#include "gl_texture.h"
#include "shader/atlas.hh"
#include "shader/draw.hh"
#include "gl_main.h"
#include "r_drawlist.h"
#include "i_system.h"
#include "con_console.h"
#include "z_zone.h"

static float envcolor[4] = { 0, 0, 0, 0 };

drawlist_t drawlist[NUMDRAWLISTS];
vtx_t drawVertex[MAXDLDRAWCOUNT];

extern cvar::BoolVar r_texturecombiner;

//
// DL_AddVertexList
//

vtxlist_t *DL_AddVertexList(drawlist_t *dl) {
    vtxlist_t* list;

    list = &dl->list[dl->index];

    if(list == &dl->list[dl->max - 1]) {
        // add a new list to the array
        dl->max++;

        // allocate array
        dl->list =
            (vtxlist_t*)Z_Realloc(dl->list,
                                  dl->max * sizeof(vtxlist_t), PU_LEVEL, NULL);

        dmemset(&dl->list[dl->max - 1], 0, sizeof(vtxlist_t));

        list = &dl->list[dl->index];
    }

    list->flags = 0;
    list->texid = 0;
    list->params = 0;
    list->subsector = rendersubsector;
    list->drawindex = rendersubsectorcount;

    return &dl->list[dl->index++];
}

//
// SortDrawList
//

static int SortDrawList(const void *a, const void *b) {
    vtxlist_t *xa = (vtxlist_t *)a;
    vtxlist_t *xb = (vtxlist_t *)b;

    return xb->texid - xa->texid;
}

//
// SortSprites
//

static int SortSprites(const void *a, const void *b) {
    visspritelist_t *xa = (visspritelist_t *)((const vtxlist_t *)a)->data;
    visspritelist_t *xb = (visspritelist_t *)((const vtxlist_t *)b)->data;

    return xb->dist - xa->dist;
}

//
// SortPainterSprites
//
// Puts the sprite list back into BSP walk order -- deepest subsector last, so
// that walking the array backwards hands out the farthest first -- and within
// one subsector orders by distance, farthest first once the group is walked
// forward. That is the order the original's per-subsector sprite list ends up
// in (DOOM64-RE, R_AddSprite inserts by zdistance).
//

static int SortPainterSprites(const void *a, const void *b) {
    const vtxlist_t *xa = (const vtxlist_t *)a;
    const vtxlist_t *xb = (const vtxlist_t *)b;

    if(xa->drawindex != xb->drawindex) {
        return xa->drawindex - xb->drawindex;
    }

    return ((visspritelist_t*)xb->data)->dist - ((visspritelist_t*)xa->data)->dist;
}

//
// DL_BuildEntry
//
// Run one entry's vertex generator and stamp the atlas coordinates onto what it
// produced. What a batch *is* does not depend on when it is drawn, so both
// processing orders share this.
//
// Returns false when the entry contributed nothing; drawcount is then left
// exactly as it was found, so a caller mid-batch loses nothing.
//

static dboolean DL_BuildEntry(vtxlist_t* head, int tag, dboolean atlas,
                              dboolean(*procfunc)(vtxlist_t*, int*),
                              int* drawcount, int* palette) {
    int first = *drawcount;

    if(procfunc) {
        if(!procfunc(head, drawcount)) {
            *drawcount = first;
            return false;
        }
    }

    // Resolve the texture up front. It used to happen at draw time, which was
    // fine while a draw covered one texture; now that entries merge, each one
    // has to stamp its own vertices.
    if(tag == DLT_SPRITE) {
        // textid in sprites contains hack that stores palette index data
        *palette = head->texid >> 24;
    }

    head->texid = (head->texid & 0xffff);

    if(atlas) {
        const auto& e = (tag == DLT_SPRITE)
                        ? shader::atlas_sprite(head->texid, *palette)
                        : shader::atlas_world(head->texid);

        if(!e.valid()) {
            //
            // The entry says nothing about where this texture lives, so the
            // vertices never got their atlas coordinates -- and without this
            // they were drawn anyway, still carrying whatever the previous
            // batch had stamped on them. That samples another texture's page,
            // or empty space, which the alpha test then discards: the thing is
            // drawn, and invisible. A monster you can hear and cannot see.
            //
            // Dropping the batch is the honest answer -- it is missing either
            // way, and this way it does not corrupt whatever shares its draw.
            // The warning names it once so the cause is not silent.
            //
            static dboolean warned = false;

            if(!warned) {
                warned = true;
                CON_Warnf("DL_ProcessDrawList: %s %i has no atlas entry; "
                          "it will not be drawn. Run texatlasverify.\n",
                          tag == DLT_SPRITE ? "sprite" : "texture",
                          head->texid);
            }

            *drawcount = first;
            return false;
        }

        shader::stamp_batch(drawVertex + first, *drawcount - first,
                            e.offset, e.layer, e.width, e.height,
                            (head->flags & DLF_MIRRORS) != 0,
                            (head->flags & DLF_MIRRORT) != 0,
                            head->params >> 1);
    }

    return true;
}

//
// DL_FlushBatch
//
// Bind whatever the accumulated vertices still need from GL, and draw them.
// head is the last entry that contributed: with the atlas in play nothing here
// reads it except the sprite blend mode, and without one a batch never spans
// two textures anyway.
//

static void DL_FlushBatch(vtxlist_t* head, int tag, dboolean atlas,
                          int* drawcount, int palette,
                          dboolean* checkNightmare) {
    if(*drawcount <= 0 || !head) {
        return;
    }

    // setup texture ID
    if(tag == DLT_SPRITE) {
        int flags = ((visspritelist_t*)head->data)->spr->flags;

        GL_BindSpriteTexture(head->texid, palette);

        // villsa 12152013 - change blend states for nightmare things
        if((*checkNightmare ^ (flags & MF_NIGHTMARE))) {
            if(!*checkNightmare && (flags & MF_NIGHTMARE)) {
                dglBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);
                *checkNightmare ^= 1;
            }
            else if(*checkNightmare && !(flags & MF_NIGHTMARE)) {
                dglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                *checkNightmare ^= 1;
            }
        }
    }
    else if(!atlas) {
        GL_BindWorldTexture(head->texid, 0, 0);
    }

    // non sprite textures must repeat or mirrored-repeat
    if(tag == DLT_WALL && !atlas) {
        dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                         head->flags & DLF_MIRRORS ? GL_MIRRORED_REPEAT : GL_REPEAT);
        dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                         head->flags & DLF_MIRRORT ? GL_MIRRORED_REPEAT : GL_REPEAT);
    }

    if(!atlas) {
        // The programmable path without an atlas still takes the light level as
        // a uniform; with one it comes from the vertices.
        shader::set_sector_light(head->params);

        if(r_texturecombiner) {
            envcolor[0] = envcolor[1] = envcolor[2] = ((float)head->params / 255.0f);
            GL_SetEnvColor(envcolor);
        }
        else {
            int l = (head->params >> 1);

            GL_UpdateEnvTexture(D_RGBA(l, l, l, 0xff));
        }
    }

    dglDrawGeometry(*drawcount, drawVertex);

    // count vertex size
    if(devparm) {
        vertCount += *drawcount;
    }

    *drawcount = 0;
    head->data = NULL;
}

//
// DL_ProcessDrawList
//

void DL_ProcessDrawList(int tag, dboolean(*procfunc)(vtxlist_t*, int*)) {
    drawlist_t* dl;
    int i;
    int drawcount = 0;
    vtxlist_t* head;
    vtxlist_t* tail;
    dboolean checkNightmare = false;

    if(tag < 0 && tag >= NUMDRAWLISTS) {
        return;
    }

    dl = &drawlist[tag];

    if(dl->max > 0) {
        int palette = 0;

        if(tag != DLT_SPRITE) {
            qsort(dl->list, dl->index, sizeof(vtxlist_t), SortDrawList);
        }
        else if(dl->index >= 2) {
            qsort(dl->list, dl->index, sizeof(vtxlist_t), SortSprites);
        }

        tail = &dl->list[dl->index];

        // With the array texture in play, a batch no longer needs its own
        // texture bound, its own wrap mode or its own light level: all three
        // ride in the vertices. Consecutive entries can then be merged whatever
        // their texture, which is the whole point of the atlas.
        dboolean atlas = shader::atlas_ready();

        for(i = 0; i < dl->index; i++) {
            vtxlist_t* rover;

            head = &dl->list[i];

            // break if no data found in list
            if(!head->data) {
                break;
            }

            if(drawcount >= MAXDLDRAWCOUNT) {
                I_Error("DL_ProcessDrawList: Draw overflow by %i, tag=%i", dl->index, tag);
            }

            if(!DL_BuildEntry(head, tag, atlas, procfunc, &drawcount, &palette)) {
                continue;
            }

            rover = head + 1;

            if(tag != DLT_SPRITE) {
                if(rover != tail) {
                    // Sprites keep their own pass: the nightmare blend mode is
                    // a real state change that no vertex attribute carries.
                    if(atlas || (head->texid == rover->texid && head->params == rover->params)) {
                        continue;
                    }
                }
            }

            DL_FlushBatch(head, tag, atlas, &drawcount, palette, &checkNightmare);
        }
    }
}

//
// DL_PainterGroup
//
// Emit one subsector's worth of one list, oldest entry first. The lists are
// filled in walk order, so a group is a contiguous run at the tail; *cursor
// walks back past it and the run itself is replayed forwards, because the
// order inside a subsector is meaningful too -- the original lays a subsector's
// walls down before its ceiling and its ceiling before its floor.
//
// Geometry accumulates into one draw while the atlas is carrying the texture;
// without one there is nothing to merge, since a draw cannot span two bindings.
//

static void DL_PainterGroup(drawlist_t* dl, int tag, int di, int* cursor,
                            dboolean atlas, dboolean(*procfunc)(vtxlist_t*, int*),
                            int* drawcount, int* palette,
                            dboolean* checkNightmare,
                            vtxlist_t** pending, int* pendingtag,
                            dboolean blend, dboolean* blendstate) {
    int start = *cursor;
    int i;

    while(start > 0 && dl->list[start - 1].drawindex == di) {
        start--;
    }

    if(start == *cursor) {
        return;
    }

    //
    // Blending is a property of the class of thing being drawn, not of the
    // subsector: walls are laid down opaque, flats and sprites are blended.
    // The old path could set it once between two whole passes; here the classes
    // alternate, so it is set per group -- and a batch that spans the change
    // has to be flushed before it, since a draw carries one blend state.
    //
    if(blend != *blendstate) {
        DL_FlushBatch(*pending, *pendingtag, atlas, drawcount, *palette, checkNightmare);
        *pending = NULL;

        GL_SetState(GLSTATE_BLEND, blend);
        *blendstate = blend;
    }

    for(i = start; i < *cursor; i++) {
        vtxlist_t* head = &dl->list[i];

        if(!head->data) {
            continue;
        }

        if(*drawcount >= MAXDLDRAWCOUNT) {
            I_Error("DL_ProcessWorldPainter: Draw overflow, tag=%i", tag);
        }

        if(!DL_BuildEntry(head, tag, atlas, procfunc, drawcount, palette)) {
            continue;
        }

        if(tag == DLT_SPRITE || !atlas) {
            // A sprite is its own draw: it carries a palette and possibly the
            // nightmare blend, neither of which a vertex attribute holds.
            DL_FlushBatch(head, tag, atlas, drawcount, *palette, checkNightmare);
            *pending = NULL;
        }
        else {
            *pending = head;
            *pendingtag = tag;
        }
    }

    *cursor = start;
}

//
// DL_ProcessWorldPainter
//

void DL_ProcessWorldPainter(dboolean(*wallfunc)(vtxlist_t*, int*),
                            dboolean(*flatfunc)(vtxlist_t*, int*),
                            dboolean(*spritefunc)(vtxlist_t*, int*)) {
    drawlist_t* dlw = &drawlist[DLT_WALL];
    drawlist_t* dlf = &drawlist[DLT_FLAT];
    drawlist_t* dls = &drawlist[DLT_SPRITE];

    int wi = dlw->index;
    int fi = dlf->index;
    int si = dls->index;

    int drawcount = 0;
    int palette = 0;
    dboolean checkNightmare = false;
    dboolean atlas = shader::atlas_ready();
    vtxlist_t* pending = NULL;
    int pendingtag = DLT_WALL;

    // What R_RenderWorld leaves the pass in: walls first, and they are opaque.
    dboolean blendstate = false;

    if(si >= 2) {
        qsort(dls->list, si, sizeof(vtxlist_t), SortPainterSprites);
    }

    //
    // Walk the subsectors of this frame from the deepest to the nearest. Each
    // list is already in walk order, so the highest drawindex still unconsumed
    // sits at the tail of one of the three; that is the next subsector to lay
    // down.
    //
    while(wi > 0 || fi > 0 || si > 0) {
        int di = 0;

        if(wi > 0 && dlw->list[wi - 1].drawindex > di) {
            di = dlw->list[wi - 1].drawindex;
        }

        if(fi > 0 && dlf->list[fi - 1].drawindex > di) {
            di = dlf->list[fi - 1].drawindex;
        }

        if(si > 0 && dls->list[si - 1].drawindex > di) {
            di = dls->list[si - 1].drawindex;
        }

        if(di <= 0) {
            // Nothing left carries a live index. Anything still held is not
            // part of this frame's walk and would be drawn out of order.
            break;
        }

        DL_PainterGroup(dlw, DLT_WALL, di, &wi, atlas, wallfunc,
                        &drawcount, &palette, &checkNightmare, &pending, &pendingtag,
                        false, &blendstate);

        DL_PainterGroup(dlf, DLT_FLAT, di, &fi, atlas, flatfunc,
                        &drawcount, &palette, &checkNightmare, &pending, &pendingtag,
                        true, &blendstate);

        if(si > 0 && dls->list[si - 1].drawindex == di) {
            //
            // The geometry standing behind these sprites has to reach the
            // framebuffer before they do -- that is the whole mechanism. Flush
            // it, then let each sprite take its own draw.
            //
            DL_FlushBatch(pending, pendingtag, atlas, &drawcount, palette, &checkNightmare);
            pending = NULL;

            DL_PainterGroup(dls, DLT_SPRITE, di, &si, atlas, spritefunc,
                            &drawcount, &palette, &checkNightmare, &pending, &pendingtag,
                            true, &blendstate);
        }
    }

    // whatever the nearest subsectors left in the batch
    DL_FlushBatch(pending, pendingtag, atlas, &drawcount, palette, &checkNightmare);

    // and leave the pass the way the old one did, so what follows is unchanged
    GL_SetState(GLSTATE_BLEND, 1);
}

//
// DL_GetDrawListSize
//

int DL_GetDrawListSize(int tag) {
    int i;

    for(i = 0; i < NUMDRAWLISTS; i++) {
        drawlist_t *dl;

        if(i != tag) {
            continue;
        }

        dl = &drawlist[i];
        return dl->max * sizeof(vtxlist_t);
    }

    return 0;
}

//
// DL_BeginDrawList
//

void DL_BeginDrawList(dboolean t, dboolean a) {
    dglSetVertex(drawVertex);

    GL_SetTextureUnit(0, t);

    if(a) {
        dglTexCombColorf(GL_TEXTURE0_ARB, envcolor, GL_ADD);
    }
}

//
// DL_Init
// Intialize draw lists
//

void DL_Init(void) {
    drawlist_t *dl;
    int i;

    for(i = 0; i < NUMDRAWLISTS; i++) {
        dl = &drawlist[i];

        dl->index   = 0;
        dl->max     = 1;
        dl->list    = (vtxlist_t*) Z_Calloc(sizeof(vtxlist_t) * dl->max, PU_LEVEL, 0);
    }
}

