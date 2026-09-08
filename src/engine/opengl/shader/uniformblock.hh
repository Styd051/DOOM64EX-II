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
// The RenderView uniform block -- KEX's gl_useUniformBuffers.
//
// progs/globals_structs.inc declares twenty-one shared values as a cbuffer. The
// engine has always compiled it with EMULATE_UNIFORM_BUFFERS, which turns every
// member into a loose uniform; that works everywhere and costs one driver call
// per value per program. With the cvar on, the same declaration becomes a real
// std140 block and the values live in one buffer that every program reads.
//
// The trick that keeps this from touching every call site: a member of a block
// has no uniform location -- glGetUniformLocation returns -1 for it -- so the
// existing code would silently stop working. Program::uniform therefore hands
// back an *encoded* location for block members, and the set_* helpers below
// decode it. A call site says set_mat4(p.u_projection, m) and does not care
// which of the two worlds it is in.
//
//   >= 0   a real uniform location
//   -1     not found, and nothing is written
//   <= -2  a block member at byte offset (-loc - 2)
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_UNIFORMBLOCK_HH__
#define __SHADER_UNIFORMBLOCK_HH__

#include "prelude.hh"

namespace imp {
  namespace shader {

    class Program;

    /*!
     * Whether real uniform buffers are in use.
     *
     * Read once, at startup, from gl_useUniformBuffers. It cannot change while
     * running: it decides how every shader in the pk3 was compiled.
     */
    bool ublock_active();

    /*!
     * Settle the question for this session and, if the answer is yes, say so in
     * the log. Called before the first shader is built.
     */
    void ublock_decide();

    /*!
     * Create the buffer and learn the member offsets from a linked program.
     *
     * The offsets are asked of the driver rather than computed here. std140 is
     * a specified layout and could be laid out by hand, but hand-laid padding
     * is exactly the sort of thing that is wrong on one vendor and right on
     * three, and there is no reason to guess when the answer can be queried.
     *
     * Safe to call repeatedly; only the first program with the block is used.
     */
    void ublock_init(const Program& p);

    /*! Point a program's RenderView block at the shared buffer. */
    void ublock_bind_program(const Program& p);

    /*!
     * The encoded location of a block member, or -1 if that name is not one.
     * Always -1 when the block is not in use.
     */
    int ublock_location(StringView name);

    void set_mat4(int loc, const float* v);
    void set_float(int loc, float v);
    void set_vec2(int loc, const float* v);
    void set_vec3(int loc, const float* v);
    void set_vec4v(int loc, int count, const float* v);
    void set_int(int loc, int v);
  }
}

#endif
