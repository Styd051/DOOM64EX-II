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

#include <map>

#include "core/cvar.hh"
#include "shader/uniformblock.hh"
#include "shader/program.hh"
#include "doomtype.h"
#include "doomdef.h"
#include "shader/gl33.hh"

using namespace imp;
using namespace imp::shader;

/*
 * KEX: "Enables OpenGL uniform buffers (requires restart)". The restart is not
 * a limitation anyone chose -- the setting decides how every shader in the pk3
 * is preprocessed, so changing it after they are built would mean rebuilding
 * all of them and relocating every uniform in the engine.
 */
cvar::BoolVar gl_useuniformbuffers = false;

namespace {
  bool decided_ {};
  bool active_ {};

  /*
   * Every uniform block the pk3 declares, with the register number its own
   * begin_cbuffer carries.
   *
   * There is more than one, and that is the whole difficulty. The GLSL
   * expansion of begin_cbuffer throws the register away -- it emits
   * "layout(std140) uniform Name" and nothing else -- so on OpenGL every block
   * defaults to binding point zero. Bind one buffer there and *all* of them
   * read it: the scene's fog block would find RenderView's matrices at its own
   * offsets and mix the world with a colour made of the projection matrix.
   * Which is exactly what it did, and the world came back flat red.
   *
   * So each block is given its own binding, taken from the register the shader
   * declares, and its own buffer. The two that nothing here drives still get a
   * binding of their own -- they must simply not be left sitting on zero.
   */
  struct BlockDef {
      const char* name;
      unsigned binding;
  };

  const BlockDef BLOCKS[] = {
      { "RenderView",               0 },
      { "PostProcess_SAO",          7 },
      { "ColorPickerParams",        8 },
      { "PostProcess_Deinterleave", 10 },
      { "PostProcess_DoomFog",      15 }
  };

  const int NUM_BLOCKS = static_cast<int>(sizeof(BLOCKS) / sizeof(BLOCKS[0]));

  struct BlockState {
      GLuint buffer {};
      int size {};
      bool known {};
  };

  BlockState blocks_[NUM_BLOCKS];

  /* Member name -> (block index, byte offset). One map for the lot: a uniform
   * name is unique across the set, so which block it belongs to is a property
   * of the name and nothing has to say it twice. */
  struct Member {
      int block;
      int offset;
  };

  std::map<String, Member> offsets_;

  /* Every member of every block. Asking for a name a given program does not
   * have is free -- the driver answers GL_INVALID_INDEX and it is skipped --
   * and listing them all means a shader that starts using one later needs no
   * change here. */
  const char* const MEMBERS[] = {
      "uProjectionMatrix",
      "uModelViewMatrix",
      "uInverseProjectionMatrix",
      "uPrevProjection",
      "uInverseModelViewMatrix",
      "uPrevModelView",
      "uRotationMatrix",
      "uInverseRotationMatrix",
      "uTransposedRotationMatrix",
      "uClipMatrix",
      "uInverseClipMatrix",
      "uPrevClipMatrix",
      "uNormalMatrix",
      "uViewOrigin",
      "uViewWidth",
      "uViewHeight",
      "uZNear",
      "uZFar",
      "uInvFocalCoords",
      "uFrustumCorners",

      // PostProcess_SAO
      "uParams1",
      "uParams2",
      "uTextureSizes",

      // PostProcess_DoomFog
      "uFogColor",
      "uFogFar",
      "uFogNear",

      // ColorPickerParams
      "uScreenX",
      "uScreenY",
      "uHue",

      // PostProcess_Deinterleave
      "uEventAtFrameN",
      "uWidthFrac",
      "uHeightFrac"
  };

  const int NUM_MEMBERS = static_cast<int>(sizeof(MEMBERS) / sizeof(MEMBERS[0]));

  /*
   * The encoding packs the block index into the location alongside the offset.
   *
   * A block is at most 64 KB by specification, so sixteen bits of offset is
   * more than the format allows, and the three bits above it name the block.
   * The whole thing is then made negative and shifted past -1, which stays
   * reserved for "no such uniform".
   */
  const int OFFSET_BITS = 16;
  const int OFFSET_MASK = (1 << OFFSET_BITS) - 1;

  int encode_(int block, int offset)
  {
      return -(((block << OFFSET_BITS) | (offset & OFFSET_MASK)) + 2);
  }

  void write_(int loc, int bytes, const void* data)
  {
      int packed = -loc - 2;
      int block = packed >> OFFSET_BITS;
      int offset = packed & OFFSET_MASK;

      if (block < 0 || block >= NUM_BLOCKS)
          return;

      BlockState& b = blocks_[block];

      if (!b.buffer || offset < 0 || offset + bytes > b.size)
          return;

      glBindBuffer(GL_UNIFORM_BUFFER, b.buffer);
      glBufferSubData(GL_UNIFORM_BUFFER, offset, bytes, data);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
  }
}

bool shader::ublock_active()
{
    return active_;
}

void shader::ublock_decide()
{
    if (decided_)
        return;

    decided_ = true;

    if (!*gl_useuniformbuffers)
        return;

    // A driver that resolved the rest of 3.3 and not these would take every
    // shader in the engine down with it, so they are asked for rather than
    // assumed.
    if (!glGenBuffers || !glBufferData || !glBufferSubData
        || !glBindBufferBase || !glGetUniformIndices
        || !glGetActiveUniformsiv || !glGetUniformBlockIndex
        || !glUniformBlockBinding || !glGetActiveUniformBlockiv) {
        log::warn("No uniform buffer entry points: gl_UseUniformBuffers ignored");
        return;
    }

    active_ = true;
    log::info("Uniform buffers: RenderView is a std140 block");
}

void shader::ublock_init(const Program& p)
{
    if (!active_ || !p)
        return;

    GLuint prog = p.id();

    for (int bi = 0; bi < NUM_BLOCKS; ++bi) {
        BlockState& b = blocks_[bi];

        if (b.known)
            continue;

        GLuint index = glGetUniformBlockIndex(prog, BLOCKS[bi].name);
        if (index == GL_INVALID_INDEX)
            continue;   // this program does not carry that block; another will

        GLint bytes = 0;
        glGetActiveUniformBlockiv(prog, index, GL_UNIFORM_BLOCK_DATA_SIZE, &bytes);

        if (bytes <= 0)
            continue;

        b.known = true;
        b.size = bytes;

        glGenBuffers(1, &b.buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, b.buffer);
        glBufferData(GL_UNIFORM_BUFFER, b.size, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        glBindBufferBase(GL_UNIFORM_BUFFER, BLOCKS[bi].binding, b.buffer);

        // Which members of this block this program can account for. A name
        // belonging to another block simply is not found here, and is picked up
        // when the program carrying that block comes past.
        GLuint indices[NUM_MEMBERS] {};
        glGetUniformIndices(prog, NUM_MEMBERS, MEMBERS, indices);

        GLuint valid[NUM_MEMBERS] {};
        int map_back[NUM_MEMBERS] {};
        int n = 0;

        for (int i = 0; i < NUM_MEMBERS; ++i) {
            if (indices[i] == GL_INVALID_INDEX)
                continue;

            // A program may carry two blocks, so the member has to be checked
            // against the one being described rather than assumed.
            GLint owner = -1;
            glGetActiveUniformsiv(prog, 1, &indices[i], GL_UNIFORM_BLOCK_INDEX, &owner);

            if (owner != static_cast<GLint>(index))
                continue;

            valid[n] = indices[i];
            map_back[n] = i;
            ++n;
        }

        if (n > 0) {
            GLint member_offsets[NUM_MEMBERS] {};
            glGetActiveUniformsiv(prog, n, valid, GL_UNIFORM_OFFSET, member_offsets);

            for (int i = 0; i < n; ++i)
                offsets_.emplace(MEMBERS[map_back[i]], Member { bi, member_offsets[i] });
        }

        log::info("Uniform block {}: binding {}, {} bytes, {} members",
                  BLOCKS[bi].name, BLOCKS[bi].binding, b.size, n);
    }
}

void shader::ublock_bind_program(const Program& p)
{
    if (!active_ || !p)
        return;

    ublock_init(p);

    // Every block this program carries, whether or not anything drives it.
    // A block left on its default binding would share point zero with
    // RenderView and read its bytes as its own.
    for (int bi = 0; bi < NUM_BLOCKS; ++bi)
        p.bind_uniform_block(BLOCKS[bi].name, BLOCKS[bi].binding);
}

int shader::ublock_location(StringView name)
{
    if (!active_)
        return -1;

    auto it = offsets_.find(name.to_string());
    if (it == offsets_.end())
        return -1;

    return encode_(it->second.block, it->second.offset);
}

void shader::set_mat4(int loc, const float* v)
{
    if (loc >= 0) {
        glUniformMatrix4fv(loc, 1, GL_FALSE, v);
        return;
    }

    if (loc <= -2)
        write_(loc, 16 * static_cast<int>(sizeof(float)), v);
}

void shader::set_float(int loc, float v)
{
    if (loc >= 0) {
        glUniform1f(loc, v);
        return;
    }

    if (loc <= -2)
        write_(loc, static_cast<int>(sizeof(float)), &v);
}

void shader::set_vec2(int loc, const float* v)
{
    if (loc >= 0) {
        glUniform2fv(loc, 1, v);
        return;
    }

    if (loc <= -2)
        write_(loc, 2 * static_cast<int>(sizeof(float)), v);
}

void shader::set_vec3(int loc, const float* v)
{
    if (loc >= 0) {
        glUniform3fv(loc, 1, v);
        return;
    }

    // Twelve bytes, not sixteen: std140 aligns a vec3 to sixteen but its size
    // is still three floats, and the four bytes after it belong to whatever
    // member the layout put there.
    if (loc <= -2)
        write_(loc, 3 * static_cast<int>(sizeof(float)), v);
}

void shader::set_vec4v(int loc, int count, const float* v)
{
    if (loc >= 0) {
        glUniform4fv(loc, count, v);
        return;
    }

    // An array of vec4 is the one case where std140 and a flat float array
    // agree: the stride is already sixteen bytes.
    if (loc <= -2)
        write_(loc, count * 4 * static_cast<int>(sizeof(float)), v);
}

void shader::set_int(int loc, int v)
{
    if (loc >= 0) {
        glUniform1i(loc, v);
        return;
    }

    if (loc <= -2)
        write_(loc, static_cast<int>(sizeof(int)), &v);
}
