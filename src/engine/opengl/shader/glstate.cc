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

#include <cmath>

#include "doomtype.h"
#include "doomdef.h"

#include "shader/glstate.hh"
#include "shader/gl33.hh"
#include "gl_main.h"

using namespace imp;
using namespace imp::shader;

namespace {
  bool fog_enabled_ {};
  unsigned fog_mode_ = GL_LINEAR;
  float fog_start_ = 0.0f;
  float fog_end_ = 1.0f;
  float fog_density_ = 1.0f;
  float fog_color_[3] {};

  // GL's defaults, so the mirror starts where the driver does.
  bool alpha_test_ {};
  // GL_TEXTURE_2D is per unit; the engine drives up to four of them.
  constexpr unsigned MAX_UNITS = 4;
  bool texture_2d_[MAX_UNITS] {};
  unsigned active_unit_ = 0;
  unsigned polygon_mode_ = GL_FILL;
  unsigned alpha_func_ = GL_ALWAYS;
  float alpha_ref_ = 0.0f;
}

void shader::state_set_enabled(unsigned cap, bool on)
{
    switch (cap) {
    case GL_FOG:
        fog_enabled_ = on;
        break;

    case GL_ALPHA_TEST:
        alpha_test_ = on;
        break;

    case GL_TEXTURE_2D:
        if (active_unit_ < MAX_UNITS)
            texture_2d_[active_unit_] = on;
        break;

    default:
        break;
    }
}

//
// Caps the core profile removed. Passing one to glEnable there is a
// GL_INVALID_ENUM, once per call -- and GL_TEXTURE_2D alone is set 34 times over
// the engine, several of them per frame.
//
static bool legacy_cap_(unsigned cap)
{
    switch (cap) {
    case GL_TEXTURE_2D:
    case GL_FOG:
    case GL_ALPHA_TEST:
        return true;

    default:
        return false;
    }
}

void shader::state_enable(unsigned cap, bool on)
{
    state_set_enabled(cap, on);

    // The fixed-function capabilities -- GL_TEXTURE_2D, GL_FOG, GL_ALPHA_TEST --
    // stop at the mirror above. The shaders read them from there; OpenGL has no
    // opinion on any of the three any more.
    if (legacy_cap_(cap))
        return;

    if (on)
        glEnable(cap);
    else
        glDisable(cap);
}

void shader::state_select_texture(unsigned texture)
{
    // glActiveTexture has been core since 1.3, so this one entry point serves
    // both profiles; glActiveTextureARB only ever existed as an extension.
    glActiveTexture(texture);
    state_active_texture(texture - GL_TEXTURE0_ARB);
}

void shader::state_set_polygon_mode(unsigned face, unsigned mode)
{
    (void)face;
    polygon_mode_ = mode;
}

unsigned shader::state_polygon_mode()
{ return polygon_mode_; }

void shader::state_fog_param(unsigned pname, float value)
{
    switch (pname) {
    case GL_FOG_MODE:
        fog_mode_ = static_cast<unsigned>(value);
        break;

    case GL_FOG_START:
        fog_start_ = value;
        break;

    case GL_FOG_END:
        fog_end_ = value;
        break;

    case GL_FOG_DENSITY:
        fog_density_ = value;
        break;

    default:
        break;
    }
}

void shader::state_fog_paramv(unsigned pname, const float* values)
{
    if (pname != GL_FOG_COLOR || !values)
        return;

    fog_color_[0] = values[0];
    fog_color_[1] = values[1];
    fog_color_[2] = values[2];
}

void shader::state_alpha_func(unsigned func, float ref)
{
    alpha_func_ = func;
    alpha_ref_ = ref;
}

bool shader::state_fog_enabled()
{ return fog_enabled_; }

unsigned shader::state_fog_mode()
{ return fog_mode_; }

float shader::state_fog_start()
{ return fog_start_; }

float shader::state_fog_end()
{ return fog_end_; }

float shader::state_fog_density()
{ return fog_density_; }

const float* shader::state_fog_color()
{ return fog_color_; }

bool shader::state_alpha_test_enabled()
{ return alpha_test_; }

void shader::state_active_texture(unsigned unit)
{
    // GL_TEXTURE0 is 0x84C0; callers pass the enum, not the index.
    unsigned index = (unit >= 0x84C0u) ? (unit - 0x84C0u) : unit;
    active_unit_ = index;
}

bool shader::state_texture_enabled(unsigned unit)
{ return (unit < MAX_UNITS) ? texture_2d_[unit] : false; }

unsigned shader::state_alpha_test_func()
{ return alpha_func_; }

float shader::state_alpha_test_ref()
{ return alpha_ref_; }

bool shader::state_selftest()
{
    // Nothing left to compare against, for the same reason matrix_selftest has
    // nothing: the fog, the alpha test and GL_TEXTURE_2D stop at the mirror now.
    // OpenGL holds none of the three, so reading it back would only measure its
    // own absence.
    //
    // This is what proved the mirror correct while both existed -- it is how
    // GL_TEXTURE_2D turned out to be per texture unit rather than global, after
    // fifty per cent of the comparisons disagreed.
    //
    // Kept because -glstatecheck still calls it, and because a second state path
    // would get checked right here.
    return true;
}

