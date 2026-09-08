// -*- mode: c++ -*-
//
// Copyright(C) 2026 Dylan (Styd051)
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION: A mirror of the fixed-function state the shaders still read.
//
// Two pieces of state survive only in the compatibility profile, and both are
// things a shader has to reproduce itself:
//
//   GL_FOG         -- the shaders compute their own fog, but need the mode,
//                     colour and distances the engine set
//   GL_ALPHA_TEST  -- becomes a discard in the fragment shader
//
// Today the shaders read them back with glGet, which the core profile will not
// answer. So the dgl* macros feed this mirror alongside the real calls, exactly
// as the matrix stack does, and state_selftest checks the two agree while both
// exist. Anything the mirror does not care about passes straight through.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_GLSTATE__48210773
#define __SHADER_GLSTATE__48210773

#include <prelude.hh>

namespace imp {
  namespace shader {
    /*! Mirrors glActiveTexture, so per-unit state lands on the right unit. */
    void state_active_texture(unsigned unit);

    /*! Mirrors glEnable / glDisable. Ignores caps it doesn't track. */
    void state_set_enabled(unsigned cap, bool on);

    /*!
     * Mirror it, then pass it to OpenGL -- unless the core profile has no such
     * cap, in which case only the mirror hears about it.
     *
     * GL_TEXTURE_2D, GL_FOG and GL_ALPHA_TEST are gone from core 3.3. They are
     * still the way the engine expresses its intent, and the shaders read that
     * intent back out of the mirror, so the calls stay where they are: this is
     * the one place that knows they must not reach the driver.
     */
    void state_enable(unsigned cap, bool on);

    /*! Set the active texture unit through the core entry point, and mirror it. */
    void state_select_texture(unsigned texture);

    /*!
     * Mirror glPolygonMode. The immediate-mode emulation needs it: a quad it
     * splits into two triangles would show the shared edge in wireframe, which
     * a real GL_QUAD never did.
     */
    void state_set_polygon_mode(unsigned face, unsigned mode);

    /*! GL_FILL or GL_LINE. */
    unsigned state_polygon_mode();

    /*! Mirrors glFogi / glFogf. */
    void state_fog_param(unsigned pname, float value);

    /*! Mirrors glFogfv, for GL_FOG_COLOR. */
    void state_fog_paramv(unsigned pname, const float* values);

    /*! Mirrors glAlphaFunc. */
    void state_alpha_func(unsigned func, float ref);

    bool state_fog_enabled();

    /*! GL_LINEAR or GL_EXP. */
    unsigned state_fog_mode();

    float state_fog_start();
    float state_fog_end();
    float state_fog_density();

    /*! Three floats, RGB. */
    const float* state_fog_color();

    bool state_alpha_test_enabled();

    /*!
     * Whether GL_TEXTURE_2D is on for a given unit. It is per-unit state, so a
     * single flag cannot represent it -- the self-test caught that immediately.
     *
     * @param unit Texture unit, 0 based
     */
    bool state_texture_enabled(unsigned unit);
    unsigned state_alpha_test_func();
    float state_alpha_test_ref();

    /*!
     * Compare the mirror against what OpenGL holds. Only meaningful while the
     * fixed-function state still exists.
     *
     * @return true if they agree
     */
    bool state_selftest();
  }
}

#endif //__SHADER_GLSTATE__48210773
