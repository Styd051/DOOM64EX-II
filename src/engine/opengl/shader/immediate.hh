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
// DESCRIPTION: glBegin/glEnd, without glBegin/glEnd.
//
// Ten places still build geometry a vertex at a time -- automap lines, console
// and menu rectangles, the debug boxes in r_main and r_things. The core profile
// has no immediate mode at all, so these gather into a vertex array instead and
// go out through the same path as everything else, which already picks between
// the fixed and programmable pipelines on its own.
//
// The dgl* macros are pointed here, so not one of the ten call sites changes.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_IMMEDIATE__72104538
#define __SHADER_IMMEDIATE__72104538

#include <prelude.hh>

namespace imp {
  namespace shader {
    /*! GL_LINES, GL_TRIANGLES or GL_POLYGON. */
    void imm_begin(unsigned mode);

    void imm_vertex(float x, float y, float z);
    void imm_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
    void imm_colorv(const unsigned char* rgba);
    void imm_colorf(float r, float g, float b, float a);
    void imm_texcoord(float u, float v);

    /*! Turn what was gathered into triangles or lines and draw it. */
    void imm_end();

    /*!
     * glRectf / glRecti, which draw a quad in the current colour and do not
     * exist in a core profile either. Six of the nine call sites are the menu
     * panels, one is the berserk flash, one the console background.
     */
    void imm_rect(float x1, float y1, float x2, float y2);
  }
}

#endif //__SHADER_IMMEDIATE__72104538
