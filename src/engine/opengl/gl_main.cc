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
// DESCRIPTION: OpenGL backend system
//
//-----------------------------------------------------------------------------

#include <math.h>
#include "SDL.h"

#ifdef HAVE_GLBINDING_3
#include <glbinding/glbinding.h>
#endif

#include "image/image.hh"

#include "doomdef.h"
#include "doomstat.h"
#include "gl_main.h"
#include "i_system.h"
#include "z_zone.h"
#include "r_main.h"
#include "gl_texture.h"
#include "con_console.h"
#include "m_misc.h"
#include "g_actions.h"

#include <string>

#include "shader/program.hh"
#include "shader/gl33.hh"
#include "system/ivideo.hh"

int ViewWindowX = 0;
int ViewWindowY = 0;
int ViewWidth   = 0;
int ViewHeight  = 0;

int gl_max_texture_units;
int gl_max_texture_size;
dboolean gl_has_combiner;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;

static float glScaleFactor = 1.0f;

dboolean    usingGL = false;
GLenum      DGL_CLAMP = GL_CLAMP;
float       max_anisotropic = 0;
dboolean    widescreen = false;

extern cvar::BoolVar r_filter;
extern cvar::BoolVar r_texturecombiner;
extern cvar::BoolVar r_anisotropic;
extern cvar::BoolVar st_flashoverlay;
extern cvar::BoolVar st_hudlinearfilter;
extern cvar::BoolVar v_vsync;
extern cvar::IntVar v_depthsize;
extern cvar::IntVar v_buffersize;
extern cvar::IntVar r_colorscale;

//
// GL_ExtensionString
//
// glGetString(GL_EXTENSIONS) answers nothing in a core profile -- it was
// replaced in 3.0 by an indexed query. Both are folded back into the one
// space-separated string the rest of this file expects.
//

static const char *GL_ExtensionString(void) {
    static std::string cached;
    static dboolean built = false;

    if(built) {
        return cached.c_str();
    }

    built = true;


    if(!glGetStringi) {
        // Asked before gl33::load(). Nothing legitimate does that, but an
        // empty list is a great deal better than a null call.
        CON_Warnf("GL_ExtensionString: glGetStringi not resolved yet\n");
        return cached.c_str();
    }

    GLint count = 0;
    dglGetIntegerv(GL_NUM_EXTENSIONS, &count);

    for(GLint i = 0; i < count; i++) {
        const GLubyte *name = glGetStringi(GL_EXTENSIONS, (GLuint)i);
        if(!name) {
            continue;
        }

        if(!cached.empty()) {
            cached += ' ';
        }
        cached += (const char *)name;
    }

    return cached.c_str();
}

//
// CMD_DumpGLExtensions
//

static CMD(DumpGLExtensions) {
    char *string;
    int i = 0;
    int len = 0;

    std::string copy = GL_ExtensionString();
    string = &copy[0];
    len = (int)copy.size();

    for(i = 0; i < len; i++) {
        if(string[i] == 0x20) {
            string[i] = '\n';
        }
    }

    M_WriteTextFile("GL_EXTENSIONS.TXT", string, len);
    CON_Printf(WHITE, "Written GL_EXTENSIONS.TXT\n");
}

//
// FindExtension
//

static dboolean FindExtension(const char *ext) {
    const char *extensions = NULL;
    const char *start;
    const char *where, *terminator;

    // Extension names should not have spaces.
    where = strrchr(ext, ' ');
    if(where || *ext == '\0') {
        return 0;
    }

    extensions = GL_ExtensionString();

    start = extensions;
    for(;;) {
        where = strstr(start, ext);
        if(!where) {
            break;
        }
        terminator = where + dstrlen(ext);
        if(where == start || *(where - 1) == ' ') {
            if(*terminator == ' ' || *terminator == '\0') {
                return true;
            }
            start = terminator;
        }
    }
    return false;
}

//
// GL_CheckExtension
//

dboolean GL_CheckExtension(const char *ext) {
    if(FindExtension(ext)) {
        CON_Printf(WHITE, "GL Extension: %s = true\n", ext);
        return true;
    }
    else {
        CON_Printf(YELLOW, "GL Extension: %s = false\n", ext);
    }

    return false;
}

//
// GL_RegisterProc
//

void* GL_RegisterProc(const char *address) {
    void *proc = SDL_GL_GetProcAddress(address);

    if(!proc) {
        CON_Warnf("GL_RegisterProc: Failed to get proc address: %s", address);
        return NULL;
    }

    return proc;
}

//
// GL_SetOrtho
//

static byte checkortho = 0;

void GL_SetOrtho(dboolean stretch) {
    float width;
    float height;

    if(checkortho) {
        if(widescreen) {
            if(stretch && checkortho == 2) {
                return;
            }
        }
        else {
            return;
        }
    }

    dglMatrixMode(GL_MODELVIEW);
    dglLoadIdentity();
    dglMatrixMode(GL_PROJECTION);
    dglLoadIdentity();

    if(widescreen && !stretch) {
        const float ratio = (4.0f / 3.0f);
        float fitwidth = ViewHeight * ratio;
        float fitx = (ViewWidth - fitwidth) / 2.0f;

        dglViewport(ViewWindowX + (int)fitx, ViewWindowY, (int)fitwidth, ViewHeight);
    }

    width = SCREENWIDTH;
    height = SCREENHEIGHT;

    if(glScaleFactor != 1.0f) {
        width /= glScaleFactor;
        height /= glScaleFactor;
    }

    dglOrtho(0, width, height, 0, -1, 1);

    checkortho = (stretch && widescreen) ? 2 : 1;
}

//
// GL_ResetViewport
//

void GL_ResetViewport(void) {
    if(widescreen) {
        dglViewport(ViewWindowX, ViewWindowY, ViewWidth, ViewHeight);
    }
}

//
// GL_SetOrthoScale
//

void GL_SetOrthoScale(float scale) {
    glScaleFactor = scale;
    checkortho = 0;
}

//
// GL_GetOrthoScale
//

float GL_GetOrthoScale(void) {
    return glScaleFactor;
}


//
// GL_SwapBuffers
//

void GL_SwapBuffers(void) {
    // -screenshot <n> writes sshot000.png on the nth presented frame, then
    // carries on. The engine can otherwise only be made to take one from a
    // key binding, which is no help when it is being driven non-interactively.
    {
        static int shotframe = -2;
        static int frame = 0;

        if(shotframe == -2) {
            int p = M_CheckParm("-screenshot");
            shotframe = (p && p < myargc - 1) ? datoi(myargv[p + 1]) : -1;
        }

        if(shotframe > 0 && ++frame == shotframe) {
            M_ScreenShot();
        }
    }

    // -shottic <n> does the same on the nth tic of the level instead of the nth
    // frame, and quits afterwards.
    //
    // That distinction is the whole point. Frames are not comparable between two
    // runs: the framerate decides how many of them fit into one tic, so turning
    // an effect on shifts which moment of the game a given frame lands on, and
    // the two images differ for reasons that have nothing to do with the effect.
    //
    // It counts leveltime and not gametic, because gametic includes everything
    // before the level started -- the wad load, the texture upload, the shader
    // compiles -- and none of that takes the same time twice. Two runs then
    // reach a given gametic at different points of the level. leveltime is reset
    // when the level begins, so the same value is the same moment in it.
    {
        static int shottic = -2;
        static dboolean taken = false;

        if(shottic == -2) {
            int p = M_CheckParm("-shottic");
            shottic = (p && p < myargc - 1) ? datoi(myargv[p + 1]) : -1;
        }

        if(shottic > 0 && !taken && leveltime >= shottic) {
            taken = true;
            M_ScreenShot();

            // And leave. Its only use is capturing a frame without a person at
            // the keyboard, and the alternative -- letting the run go on until
            // something outside kills it -- means the engine never reaches
            // M_SaveDefaults, so config.cfg is left truncated and the next run
            // starts from a different state. That is not a tidiness problem: it
            // silently destroys any comparison between two runs.
            I_Quit();
        }
    }


    // -glcheck drains the error queue once a frame and reports each distinct
    // code the first time it appears.
    //
    // The core profile has no way of complaining otherwise: a call it removed
    // raises GL_INVALID_ENUM and returns, the frame is drawn without it, and
    // nothing is ever said. This is how the removed calls get found.
    {
        static int check = -1;
        static dboolean seen[8];

        if(check < 0) {
            check = M_CheckParm("-glcheck") ? 1 : 0;
            dmemset(seen, 0, sizeof(seen));
        }

        if(check) {
            GLenum err;
            int drained = 0;

            // Bounded: a driver that keeps answering would hang the frame.
            while((err = dglGetError()) != GL_NO_ERROR && drained++ < 64) {
                unsigned slot = (unsigned)(err - GL_INVALID_ENUM);

                if(slot >= 8) {
                    slot = 7;
                }

                if(!seen[slot]) {
                    seen[slot] = true;
                    CON_Warnf("GL error 0x%x raised during frame\n", (unsigned)err);
                }
            }
        }
    }

    Video->end_frame();
}

//
// GL_GetScreenBuffer
//

Image GL_GetScreenBuffer(int16 x, int16 y, uint16 width, uint16 height) {
    RgbImage image { width, height };
    int pack;

    //
    // 20120313 villsa - force pack alignment to 1
    //
    dglGetIntegerv(GL_PACK_ALIGNMENT, &pack);
    dglPixelStorei(GL_PACK_ALIGNMENT, 1);
    dglFlush();
    dglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, image.data_ptr());
    dglPixelStorei(GL_PACK_ALIGNMENT, pack);

    Image img = std::move(image);
    img.flip_vertical();

    return img;
}

//
// GL_SetTextureFilter
//

void GL_SetTextureFilter(void) {
    if(!usingGL) {
        return;
    }

    dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, r_filter==0 ? GL_LINEAR : GL_NEAREST);
    dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, r_filter==0 ? GL_LINEAR : GL_NEAREST);

    if(GLAD_GL_EXT_texture_filter_anisotropic) {
        if(r_anisotropic) {
            dglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_anisotropic);
        }
        else {
            // 1, not 0: the legal range is [1, max], and 0 is a
            // GL_INVALID_VALUE. 1 is what "no anisotropic filtering" means.
            dglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
        }
    }
}

//
// GL_SetTextureFilterHud
//
// [kex] st_HudLinearFilter.
//
// r_Filter picks one filter for everything the engine uploads, and linear is the
// right answer for the world: a wall seen at an angle has no pixel grid of its
// own to preserve, so smoothing between texels is what you want. The interface
// is the opposite case. The status bar, the fonts and the menu graphics are
// authored at the N64's own 320x240 and then stretched whole to the window, one
// grid on to another; filtering between them only softens the letters and puts a
// halo round the numbers. So it gets its own switch.
//
// Three places in the engine had already worked this out and hard-coded
// GL_NEAREST -- the console font, the Japanese messages, and the screen capture
// in GL_ScreenToTexture. This is the same decision, made once, and made
// answerable. Set at draw time rather than baked at upload, which is why it
// needs no GL_DumpTextures callback the way r_Filter does: it takes effect on
// the next frame.
//
// Anisotropy is deliberately not touched. It only ever applies to a minified
// texture at an angle, which no part of a flat interface quad ever is.
//

void GL_SetTextureFilterHud(void) {
    if(!usingGL) {
        return;
    }

    dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                     st_hudlinearfilter ? GL_LINEAR : GL_NEAREST);
    dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     st_hudlinearfilter ? GL_LINEAR : GL_NEAREST);
}

//
// GL_SetDefaultCombiner
//

void GL_SetDefaultCombiner(void) {
    if (!usingGL) {
        return;
    }

    if(GLAD_GL_ARB_multitexture) {
        GL_SetTextureUnit(1, false);
        GL_SetTextureUnit(2, false);
        GL_SetTextureUnit(3, false);
        GL_SetTextureUnit(0, true);
    }

    GL_CheckFillMode();

    if(r_texturecombiner > 0) {
        dglTexCombModulate(GL_TEXTURE0_ARB, GL_PRIMARY_COLOR);
    }
    else {
        GL_SetTextureMode(GL_MODULATE);
    }
}

//
// GL_SetColorScale
//

void GL_SetColorScale(void) {
    if (!usingGL) {
        return;
    }

    int cs = *r_colorscale;

    switch(cs) {
        case 1:
            dglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);
            break;
        case 2:
            dglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 4);
            break;
        default:
            dglTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
            break;
    }
}

//
// GL_Set2DQuad
//

void GL_Set2DQuad(vtx_t *v, float x, float y, int width, int height,
                  float u1, float u2, float v1, float v2, rcolor c) {
    float left, right, top, bottom;

    left = ViewWindowX + x * ViewWidth / video_width;
    right = left + (width * ViewWidth / video_width);
    top = ViewWindowY + y * ViewHeight / video_height;
    bottom = top + (height * ViewHeight / video_height);

    v[0].x = v[2].x = left;
    v[1].x = v[3].x = right;
    v[0].y = v[1].y = top;
    v[2].y = v[3].y = bottom;

    v[0].z = v[1].z = v[2].z = v[3].z = 0.0f;

    v[0].tu = u1;
    v[2].tu = u1;
    v[1].tu = u2;
    v[3].tu = u2;
    v[0].tv = v1;
    v[1].tv = v1;
    v[2].tv = v2;
    v[3].tv = v2;

    dglSetVertexColor(v, c, 4);
}

//
// GL_Draw2DQuad
//

void GL_Draw2DQuad(vtx_t *v, dboolean stretch) {
    GL_SetOrtho(stretch);

    dglSetVertex(v);
    dglTriangle(0, 1, 2);
    dglTriangle(3, 2, 1);
    dglDrawGeometry(4, v);

    GL_ResetViewport();

    if(devparm) {
        vertCount += 4;
    }
}

//
// GL_SetupAndDraw2DQuad
//

void GL_SetupAndDraw2DQuad(float x, float y, int width, int height,
                           float u1, float u2, float v1, float v2, rcolor c, dboolean stretch) {
    vtx_t v[4];

    GL_Set2DQuad(v, x, y, width, height, u1, u2, v1, v2, c);
    GL_Draw2DQuad(v, stretch);
};

//
// GL_SetState
//

static int glstate_flag = 0;

void GL_SetState(int bit, dboolean enable) {
#define TOGGLEGLBIT(flag, bit)                          \
    if(enable && !(glstate_flag & (1 << flag)))         \
    {                                                   \
        dglEnable(bit);                                 \
        glstate_flag |= (1 << flag);                    \
    }                                                   \
    else if(!enable && (glstate_flag & (1 << flag)))    \
    {                                                   \
        dglDisable(bit);                                \
        glstate_flag &= ~(1 << flag);                   \
    }

    switch(bit) {
    case GLSTATE_BLEND:
        TOGGLEGLBIT(GLSTATE_BLEND, GL_BLEND);
        break;
    case GLSTATE_CULL:
        TOGGLEGLBIT(GLSTATE_CULL, GL_CULL_FACE);
        break;
    case GLSTATE_TEXTURE0:
        TOGGLEGLBIT(GLSTATE_TEXTURE0, GL_TEXTURE_2D);
        break;
    case GLSTATE_TEXTURE1:
        TOGGLEGLBIT(GLSTATE_TEXTURE1, GL_TEXTURE_2D);
        break;
    case GLSTATE_TEXTURE2:
        TOGGLEGLBIT(GLSTATE_TEXTURE2, GL_TEXTURE_2D);
        break;
    case GLSTATE_TEXTURE3:
        TOGGLEGLBIT(GLSTATE_TEXTURE3, GL_TEXTURE_2D);
        break;
    default:
        CON_Warnf("GL_SetState: unknown bit flag: %i\n", bit);
        break;
    }

#undef TOGGLEGLBIT
}

//
// GL_CheckFillMode
//

void GL_CheckFillMode(void) {
    GL_SetState(GLSTATE_TEXTURE0, !r_fillmode ? 0 : 1);
}

//
// GL_ClearView
//

void GL_ClearView(rcolor clearcolor) {
    float f[4];

    dglGetColorf(clearcolor, f);
    dglClearColor(f[0], f[1], f[2], f[3]);
    dglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    dglViewport(ViewWindowX, ViewWindowY, ViewWidth, ViewHeight);
    dglScissor(ViewWindowX, ViewWindowY, ViewWidth, ViewHeight);
}

//
// GL_CalcViewSize
//

void GL_CalcViewSize(void) {
    ViewWidth = video_width;
    ViewHeight = video_height;

    widescreen = !dfcmp(((float)ViewWidth / (float)ViewHeight), (4.0f / 3.0f));

    ViewWindowX = (video_width - ViewWidth) / 2;

    if(ViewWidth == video_width) {
        ViewWindowY = 0;
    }
    else {
        ViewWindowY = (ViewHeight) / 2;
    }
}

//
// GetVersionInt
// Borrowed from prboom+
//

typedef enum {
    OPENGL_VERSION_1_0,
    OPENGL_VERSION_1_1,
    OPENGL_VERSION_1_2,
    OPENGL_VERSION_1_3,
    OPENGL_VERSION_1_4,
    OPENGL_VERSION_1_5,
    OPENGL_VERSION_2_0,
    OPENGL_VERSION_2_1,
} glversion_t;

static int GetVersionInt(const char* version) {
    int MajorVersion;
    int MinorVersion;
    int versionvar;

    versionvar = OPENGL_VERSION_1_0;

    if(sscanf(version, "%d.%d", &MajorVersion, &MinorVersion) == 2) {
        if(MajorVersion > 1) {
            versionvar = OPENGL_VERSION_2_0;

            if(MinorVersion > 0) {
                versionvar = OPENGL_VERSION_2_1;
            }
        }
        else {
            versionvar = OPENGL_VERSION_1_0;

            if(MinorVersion > 0) {
                versionvar = OPENGL_VERSION_1_1;
            }
            if(MinorVersion > 1) {
                versionvar = OPENGL_VERSION_1_2;
            }
            if(MinorVersion > 2) {
                versionvar = OPENGL_VERSION_1_3;
            }
            if(MinorVersion > 3) {
                versionvar = OPENGL_VERSION_1_4;
            }
            if(MinorVersion > 4) {
                versionvar = OPENGL_VERSION_1_5;
            }
        }
    }

    return versionvar;
}

//
// GL_Init
//

void GL_Init(void) {
#ifdef HAVE_GLBINDING_3
    glbinding::initialize([](const char *proc) { return reinterpret_cast<glbinding::ProcAddress>(SDL_GL_GetProcAddress(proc)); });
#else
    gladLoadGLLoader(SDL_GL_GetProcAddress);
#endif


    // Asked for the log line it prints, and because a driver is free to hand
    // back something other than what SDL requested. Nothing branches on it any
    // more; it is a statement of fact about the context we ended up with.
    imp::gl33::core_profile();

    gl_vendor = dglGetString(GL_VENDOR);
    I_Printf("GL_VENDOR: %s\n", gl_vendor);
    gl_renderer = dglGetString(GL_RENDERER);
    I_Printf("GL_RENDERER: %s\n", gl_renderer);
    gl_version = dglGetString(GL_VERSION);
    I_Printf("GL_VERSION: %s\n", gl_version);
    dglGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);
    I_Printf("GL_MAX_TEXTURE_SIZE: %i\n", gl_max_texture_size);
    // GL_MAX_TEXTURE_UNITS_ARB counted fixed-function combiner units, which the
    // core profile no longer has. GL_MAX_TEXTURE_IMAGE_UNITS is its
    // replacement: how many samplers a fragment shader may bind.
    dglGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &gl_max_texture_units);
    I_Printf("GL_MAX_TEXTURE_IMAGE_UNITS: %i\n", gl_max_texture_units);

    // The extension rescan that used to sit here is gone. It existed because
    // glad was generated for 1.4 and scanned with a single
    // glGetString(GL_EXTENSIONS), which a core profile refuses -- leaving every
    // GLAD_GL_* false and, through r_TexNonPowResize, every sprite in the game
    // one power of two too big. Glad's 3.3 loader scans with glGetStringi and
    // gets it right, and the five flags that no longer exist are constants in
    // gl_main.h.

    if(gl_max_texture_units <= 2) {
        CON_Warnf("Not enough texture units supported...\n");
    }

    GL_CalcViewSize();

    dglViewport(0, 0, video_width, video_height);
    dglClearDepth(1.0f);
    dglDisable(GL_TEXTURE_2D);
    dglEnable(GL_CULL_FACE);
    dglCullFace(GL_FRONT);
    dglShadeModel(GL_SMOOTH);
    // Both hints were removed in the core profile; GL_FOG_HINT went with fog
    // itself, and perspective correction is no longer optional.
    // GL_PERSPECTIVE_CORRECTION_HINT: gone with the fixed pipeline. Core is
    // perspective-correct and has no say in the matter.
    dglDepthFunc(GL_LEQUAL);
    dglAlphaFunc(GL_GEQUAL, ALPHACLEARGLOBAL);
    dglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    dglFogi(GL_FOG_MODE, GL_LINEAR);
    // GL_FOG_HINT: likewise. The fog is computed in doomSceneMain now.
    dglEnable(GL_SCISSOR_TEST);
    dglEnable(GL_DITHER);

    usingGL = true;

    // Before the first GL_SetTextureFilter, not after: that call reads
    // max_anisotropic, and a zero there is outside the legal [1, max] range.
    if(GLAD_GL_EXT_texture_filter_anisotropic) {
        dglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropic);
    }

    GL_SetTextureFilter();
    GL_SetDefaultCombiner();
    GL_SetColorScale();

    r_fillmode = true;

    if(!GLAD_GL_ARB_multitexture) {
        CON_Warnf("GL_ARB_multitexture not supported...\n");
    }

    gl_has_combiner = (GLAD_GL_ARB_texture_env_combine | GLAD_GL_EXT_texture_env_combine);

    if(!gl_has_combiner) {
        CON_Warnf("Texture combiners not supported...\n");
        CON_Warnf("Setting r_texturecombiner to 0\n");
        r_texturecombiner = false;
    }

    dglEnableClientState(GL_VERTEX_ARRAY);
    dglEnableClientState(GL_TEXTURE_COORD_ARRAY);
    dglEnableClientState(GL_COLOR_ARRAY);

    DGL_CLAMP = (GetVersionInt(gl_version) >= OPENGL_VERSION_1_2 ? GL_CLAMP_TO_EDGE : GL_CLAMP);

    glScaleFactor = 1.0f;

    G_AddCommand("dumpglext", CMD_DumpGLExtensions, 0);

    shader::init();
}

