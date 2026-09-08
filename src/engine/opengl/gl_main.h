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

#ifndef __GL_MAIN_H__
#define __GL_MAIN_H__

// Define APIENTRY on Windows so that glad doesn't include windows.h
#if _WIN32
#define APIENTRY __stdcall
#endif

#ifdef HAVE_GLBINDING_3
#include <glbinding/gl14/gl.h>
#include <glbinding/gl14ext/gl.h>

constexpr bool GLAD_GL_ARB_multitexture               = true;
constexpr bool GLAD_GL_ARB_texture_non_power_of_two   = true;
constexpr bool GLAD_GL_ARB_texture_env_combine        = true;
constexpr bool GLAD_GL_EXT_compiled_vertex_array      = true;
constexpr bool GLAD_GL_EXT_texture_env_combine        = true;
constexpr bool GLAD_GL_EXT_texture_filter_anisotropic = true;

using namespace gl14;
using namespace gl14ext;
#else
#include "glad/glad.h"

// Glad is generated for 3.3 core now, so five of the six flags the engine used
// to consult no longer exist as variables. Two were folded into the core years
// ago and three went out with the fixed pipeline. They stay here as constants
// because the call sites read better for the name than for a bare true or false,
// and because a constant folds the dead branch away entirely.
//
// GL_EXT_texture_filter_anisotropic is the one real extension left, and glad
// still sets it -- its 3.3 loader scans with glGetStringi, which a core profile
// answers properly.
constexpr bool GLAD_GL_ARB_multitexture             = true;   // core since 1.3
constexpr bool GLAD_GL_ARB_texture_non_power_of_two = true;   // core since 2.0
constexpr bool GLAD_GL_ARB_texture_env_combine      = false;  // fixed pipeline
constexpr bool GLAD_GL_EXT_texture_env_combine      = false;  // fixed pipeline
constexpr bool GLAD_GL_EXT_compiled_vertex_array    = false;  // client arrays

// The names the fixed pipeline left behind.
//
// These are no longer OpenGL enums -- the core profile removed every call that
// took them, and glad no longer declares them. They survive as the engine's own
// vocabulary: shader/glstate.cc keys its fog and alpha-test mirror on them,
// gl_texture.cc names texture environment modes with them, and the immediate
// mode emulation still speaks of GL_QUADS and GL_POLYGON.
//
// The values are the historical ones, lifted from the 1.4 header this replaces
// rather than typed from memory, so nothing that compares or switches on them
// changes meaning.
#define GL_ALPHA_TEST                    0x0BC0
#define GL_FOG                           0x0B60
#define GL_FOG_MODE                      0x0B65
#define GL_FOG_DENSITY                   0x0B62
#define GL_FOG_START                     0x0B63
#define GL_FOG_END                       0x0B64
#define GL_FOG_COLOR                     0x0B66
#define GL_EXP                           0x0800
#define GL_EXP2                          0x0801
#define GL_MODULATE                      0x2100
#define GL_ADD                           0x0104
#define GL_COMBINE                       0x8570
#define GL_PREVIOUS                      0x8578
#define GL_TEXTURE_ENV                   0x2300
#define GL_TEXTURE_ENV_MODE              0x2200
#define GL_TEXTURE_ENV_COLOR             0x2201
#define GL_SMOOTH                        0x1D01
#define GL_FLAT                          0x1D00
#define GL_CLAMP                         0x2900
#define GL_QUADS                         0x0007
#define GL_POLYGON                       0x0009
#define GL_MAX_TEXTURE_UNITS_ARB         0x84E2
#define GL_VERTEX_ARRAY                  0x8074
#define GL_COLOR_ARRAY                   0x8076
#define GL_TEXTURE_COORD_ARRAY           0x8078
#define GL_PERSPECTIVE_CORRECTION_HINT   0x0C50
#define GL_FOG_HINT                      0x0C54
#define GL_TEXTURE0_ARB                  0x84C0
#define GL_TEXTURE1_ARB                  0x84C1
#define GL_TEXTURE2_ARB                  0x84C2
#define GL_TEXTURE3_ARB                  0x84C3

// The matrix modes, and the texture combiner vocabulary. Same reasoning: the
// engine's matrix stack still speaks of GL_PROJECTION and GL_MODELVIEW, and
// gl_texture.cc still describes the combiner it emulates in the combiner's own
// terms.
#define GL_PROJECTION                    0x1701
#define GL_MODELVIEW                     0x1700
#define GL_PRIMARY_COLOR                 0x8577
#define GL_CONSTANT                      0x8576
#define GL_COMBINE_ARB                   0x8570
#define GL_COMBINE_RGB                   0x8571
#define GL_COMBINE_ALPHA                 0x8572
#define GL_INTERPOLATE                   0x8575
#define GL_SOURCE0_RGB                   0x8580
#define GL_SOURCE1_RGB                   0x8581
#define GL_SOURCE2_RGB                   0x8582
#define GL_OPERAND0_RGB                  0x8590
#define GL_OPERAND1_RGB                  0x8591
#endif

typedef GLuint        dtexture;
typedef GLfloat        rfloat;
typedef GLuint        rcolor;
typedef GLuint        rbuffer;
typedef GLhandleARB    rhandle;

extern int gl_max_texture_units;
extern int gl_max_texture_size;
extern dboolean gl_has_combiner;

typedef struct {
    rfloat    x;
    rfloat    y;
    rfloat    z;
    rfloat    tu;
    rfloat    tv;
    byte r;
    byte g;
    byte b;
    byte a;

    //
    // Where this vertex's texture lives in the array texture, and how to read
    // it. Only the programmable world path uses these; the fixed pipeline and
    // the 2D paths leave them alone.
    //
    // They are per-batch rather than per-vertex, so DL_ProcessDrawList stamps
    // them onto a batch's vertices just before the draw, at the point where it
    // used to bind a texture. The layout matches the attributes that
    // progs/doomSceneMain.shader declares.
    //
    int  offset;        // linear texel offset within the layer   (attrib 3)
    int  dimensions;    // width << 16 | height                    (attrib 4)
    int  options;       // bit0 flipY, bit1 flipX, bits 3-10 glow  (attrib 5)
    byte fr;            // HUD flash red                           (attrib 6)
    byte fg;            // HUD flash green
    byte fb;            // HUD flash blue
    byte layer;         // array layer
} vtx_t;

#define MAXSPRPALSETS       4

typedef struct {
    byte r;
    byte g;
    byte b;
    byte a;
} dPalette_t;

#define GLSTATE_BLEND       0
#define GLSTATE_CULL        1
#define GLSTATE_TEXTURE0    2
#define GLSTATE_TEXTURE1    3
#define GLSTATE_TEXTURE2    4
#define GLSTATE_TEXTURE3    5

extern int ViewWidth;
extern int ViewHeight;
extern int ViewWindowX;
extern int ViewWindowY;

#define FIELDOFVIEW         2048            // Fineangles in the video_width wide window.
#define ALPHACLEARGLOBAL    0.01f
#define ALPHACLEARTEXTURE   0.8f
#define MAX_COORD           32767.0f

#define TESTALPHA(x)        ((byte)((x >> 24) & 0xff) < 0xff)

extern GLenum DGL_CLAMP;

extern dboolean usingGL;

void GL_CalcViewSize();
namespace imp {
  class Image;
}

dboolean GL_CheckExtension(const char *ext);
void* GL_RegisterProc(const char *address);
void GL_Init(void);
void GL_ClearView(rcolor clearcolor);
dboolean GL_GetBool(int x);
void GL_CheckFillMode(void);
void GL_SwapBuffers(void);
Image GL_GetScreenBuffer(int16 x, int16 y, uint16 width, uint16 height);
void GL_SetTextureFilter(void);
void GL_SetTextureFilterHud(void);
void GL_SetOrtho(dboolean stretch);
void GL_ResetViewport(void);
void GL_SetOrthoScale(float scale);
float GL_GetOrthoScale(void);
void GL_SetState(int bit, dboolean enable);
void GL_SetDefaultCombiner(void);
void GL_SetColorScale(void);
void GL_Set2DQuad(vtx_t *v, float x, float y, int width, int height,
                  float u1, float u2, float v1, float v2, rcolor c);
void GL_Draw2DQuad(vtx_t *v, dboolean stretch);
void GL_SetupAndDraw2DQuad(float x, float y, int width, int height,
                           float u1, float u2, float v1, float v2, rcolor c, dboolean stretch);

#endif
