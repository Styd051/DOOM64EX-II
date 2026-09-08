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

#include "doomtype.h"
#include "doomdef.h"

#include "shader/immediate.hh"
#include "shader/draw.hh"
#include "shader/glstate.hh"

using namespace imp;
using namespace imp::shader;

namespace {
  // The largest of the ten call sites emits four vertices. This is generous.
  constexpr size_t MAX_VERTS = 64;

  vtx_t verts_[MAX_VERTS];
  unsigned short indices_[MAX_VERTS * 3];

  size_t count_ {};
  unsigned mode_ {};
  bool active_ {};

  // Current colour and texcoord, as the fixed pipeline keeps them between
  // vertices.
  byte cr_ = 255, cg_ = 255, cb_ = 255, ca_ = 255;
  float tu_ {}, tv_ {};
}

void shader::imm_begin(unsigned mode)
{
    mode_ = mode;
    count_ = 0;
    active_ = true;
}

void shader::imm_vertex(float x, float y, float z)
{
    if (!active_ || count_ >= MAX_VERTS)
        return;

    vtx_t& v = verts_[count_++];

    v.x = x;
    v.y = y;
    v.z = z;
    v.tu = tu_;
    v.tv = tv_;
    v.r = cr_;
    v.g = cg_;
    v.b = cb_;
    v.a = ca_;

    // Nothing here goes through DL_ProcessDrawList, so there is no atlas
    // metadata to carry; zeroing keeps the world program from ever claiming it.
    v.offset = 0;
    v.dimensions = 0;
    v.options = 0;
    v.fr = v.fg = v.fb = v.layer = 0;
}

void shader::imm_color(byte r, byte g, byte b, byte a)
{
    cr_ = r;
    cg_ = g;
    cb_ = b;
    ca_ = a;
}

void shader::imm_colorv(const byte* rgba)
{
    if (rgba)
        imm_color(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void shader::imm_colorf(float r, float g, float b, float a)
{
    auto to_byte = [](float f) {
        if (f <= 0.0f) return static_cast<byte>(0);
        if (f >= 1.0f) return static_cast<byte>(255);
        return static_cast<byte>(f * 255.0f + 0.5f);
    };

    imm_color(to_byte(r), to_byte(g), to_byte(b), to_byte(a));
}

void shader::imm_texcoord(float u, float v)
{
    tu_ = u;
    tv_ = v;
}

void shader::imm_rect(float x1, float y1, float x2, float y2)
{
    // glRect draws the quad in the z=0 plane with the current colour, and
    // reuses the current texcoord for every corner.
    imm_begin(GL_POLYGON);
    imm_vertex(x1, y1, 0.0f);
    imm_vertex(x2, y1, 0.0f);
    imm_vertex(x2, y2, 0.0f);
    imm_vertex(x1, y2, 0.0f);
    imm_end();
}

void shader::imm_end()
{
    active_ = false;

    if (count_ < 2)
        return;

    size_t n = 0;
    unsigned prim = GL_TRIANGLES;

    switch (mode_) {
    case GL_LINES:
        prim = GL_LINES;
        for (size_t i = 0; i + 1 < count_; i += 2) {
            indices_[n++] = static_cast<unsigned short>(i);
            indices_[n++] = static_cast<unsigned short>(i + 1);
        }
        break;

    case GL_TRIANGLES:
        for (size_t i = 0; i + 2 < count_; i += 3) {
            indices_[n++] = static_cast<unsigned short>(i);
            indices_[n++] = static_cast<unsigned short>(i + 1);
            indices_[n++] = static_cast<unsigned short>(i + 2);
        }
        break;

    default:
        if (state_polygon_mode() == GL_LINE) {
            // A fan would put its shared edge on screen. glPolygonMode outlines
            // every triangle it is given, and the quad the engine asked for is
            // two of them -- so the diagonal appears, which no real GL_QUAD ever
            // showed. The menu panels in M_DrawSaveGame are drawn exactly this
            // way, and that diagonal is what crossed all three of them.
            //
            // Outline the polygon directly instead. GL_LINE_LOOP is core, and
            // polygon mode does not apply to it, so the edge cannot come back.
            prim = GL_LINE_LOOP;
            for (size_t i = 0; i < count_; ++i) {
                indices_[n++] = static_cast<unsigned short>(i);
            }
            break;
        }

        // GL_POLYGON, always convex here: a fan from the first vertex.
        for (size_t i = 1; i + 1 < count_; ++i) {
            indices_[n++] = 0;
            indices_[n++] = static_cast<unsigned short>(i);
            indices_[n++] = static_cast<unsigned short>(i + 1);
        }
        break;
    }

    if (n == 0)
        return;

    dglSetVertex(verts_);
    dglDrawGeometryPrim(static_cast<dword>(count_), verts_, indices_, n, prim);
}
