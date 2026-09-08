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

#include <cstddef>
#include <algorithm>

#include <core/cvar.hh>

// gl_main.h, reached through shader/draw.hh, names dboolean.
#include "doomtype.h"
#include "doomdef.h"
#include "m_misc.h"

#include "shader/draw.hh"
#include "shader/program.hh"
#include "gl_texture.h"
#include "shader/atlas.hh"
#include "shader/gl33.hh"
#include "shader/uniformblock.hh"
#include "shader/matrix.hh"
#include "shader/glstate.hh"

using namespace imp;
using namespace imp::shader;


/*
 * The N64's three-point texture filter, as progs/doomSceneMain.shader
 * emulates it under USE_FILTERING.
 *
 * It is a second entry point on the same shader -- doomSceneFilter rather than
 * doomSceneNoFilter -- so both are built and the cvar picks between them at
 * draw time. Off by default: it changes how every wall in the game looks, and
 * that is a decision for the player, not a silent one.
 */
cvar::BoolVar r_n64filter = false;

namespace {
  /*
   * The far plane doomSceneMain normalises depth against.
   *
   * The engine has none of its own: dglViewFrustum builds an infinite
   * projection. uZFar used to be 1, which made the shader measure depth in
   * world units -- convenient for the fog, whose distances are in world units
   * too, and harmless while nothing else read it.
   *
   * The G-buffer changes that. Its depth target is what SAO and the motion
   * blur will sample, and both expect [0, 1]. So depth is normalised properly
   * now, and the fog distances are divided by the same number on their way to
   * the shader -- the ratio is what the fog actually depends on, so it comes
   * out unchanged.
   *
   * 8192 is past anything a Doom 64 map holds. Beyond it depth simply exceeds
   * 1, which the half-float target stores without complaint.
   */
  constexpr float WORLD_FAR = 8192.0f;
}

namespace {
  /*!
   * One program and everything needed to feed it.
   *
   * There are two: world geometry reads the array texture and takes its
   * texture, mirroring and light from vertex attributes; everything else binds
   * a 2D texture and needs the fixed pipeline's texture environment. Keeping
   * them apart is what lets the world program converge on doomSceneMain, which
   * knows nothing about either.
   */
  struct ProgramState {
      Program program;

      int u_projection { -1 };
      int u_modelview { -1 };
      int u_base { -1 };
      int u_atlas { -1 };
      int u_texmode { -1 };
      int u_sector_light { -1 };
      int u_fog_params { -1 };
      int u_fog_color { -1 };

      // doomSceneMain names its fog differently and normalises depth by uZFar.
      int u_zfar { -1 };
      int u_fog_near { -1 };
      int u_fog_far { -1 };
      int u_alpha_test { -1 };

      // Only the MRT variants have these: doomSceneMain needs the previous
      // frame's matrices to work out where a fragment was, which is the whole
      // of the velocity target.
      int u_prev_projection { -1 };
      int u_prev_modelview { -1 };

      // Last values handed to GL. Uniforms live in the program object, so they
      // survive between draws and only changes need uploading.
      float last_projection[16] {};
      float last_modelview[16] {};
      float last_texmode { -1.0f };
      float last_sector_light { -1.0f };
      float last_fog_params[3] { -1.0f, -1.0f, -1.0f };
      float last_fog_color[3] { -1.0f, -1.0f, -1.0f };
      float last_alpha_test[2] { -1.0f, -1.0f };

      explicit operator bool() const
      { return static_cast<bool>(program); }
  };

  ProgramState world_;
  ProgramState world_filtered_;

  /* The same two entry points again, with HAS_MRT defined. They write the
   * three-target G-buffer instead of a single colour, so they can only be
   * used while a framebuffer with three attachments is bound. */
  ProgramState world_mrt_;
  ProgramState world_filtered_mrt_;

  ProgramState generic_;

  /* The view as it was last frame. doomSceneMain projects each vertex through
   * both and writes the difference; without this the velocity target would be
   * noise. Captured after the world is drawn, so it is genuinely one frame
   * behind. */
  float prev_projection_[16] {};
  float prev_modelview_[16] {};
  float last_projection_[16] {};
  float last_modelview_[16] {};
  bool have_prev_ {};

  GLuint vao_ {};
  GLuint vbo_ {};
  GLuint ibo_ {};

  /* Set from DL_ProcessDrawList, which knows the light level of the batch it
   * is about to emit. Only the generic program uses it; world geometry carries
   * its light in a vertex attribute. */
  float sector_light_ {};

  /* Set by stamp_batch, cleared after each draw: only geometry that went
   * through DL_ProcessDrawList carries usable atlas metadata, and only that
   * geometry may go through the world program. */
  bool atlas_batch_ {};

  bool ready_ {};
  bool probed_ {};

  /* Cached GL state, so a per-batch draw doesn't have to read it back.
   *
   * Reading state back keeps the two paths from diverging, but glGet stalls the
   * driver. Fog is only ever configured in SetupFog, which always sets the
   * parameters right after enabling fog, and R_RenderWorld disables it again at
   * the end -- so re-reading on the disabled-to-enabled edge is enough, and
   * happens once a frame. */
  bool fog_cached_ {};
  float fog_params_[3] {};
  float fog_color_[3] {};

  /* What SetupFog says the fog runs between, in world units, straight from the
   * original's own numbers. Zero until it has spoken. */
  float world_fog_near_ {};
  float world_fog_far_ {};

  bool differs_(const float* a, const float* b, size_t n)
  {
      for (size_t i {}; i < n; ++i) {
          if (a[i] != b[i])
              return true;
      }
      return false;
  }

  void remember_(float* dst, const float* src, size_t n)
  {
      for (size_t i {}; i < n; ++i)
          dst[i] = src[i];
  }

  /*! Fetch uniform locations and pin the samplers. */
  void locate_(ProgramState& p)
  {
      // Before the first lookup: ublock_init learns the member offsets from
      // the first program that carries the block, and every location asked for
      // afterwards depends on them being known.
      ublock_bind_program(p.program);

      p.u_projection   = p.program.uniform("uProjectionMatrix");
      p.u_modelview    = p.program.uniform("uModelViewMatrix");
      p.u_base         = p.program.uniform("uTex_tBase");
      p.u_atlas        = p.program.uniform("uTex_tAtlas");
      p.u_texmode      = p.program.uniform("uTexMode");
      p.u_sector_light = p.program.uniform("uSectorLight");
      p.u_fog_params   = p.program.uniform("uFogParams");
      p.u_fog_color    = p.program.uniform("uFogColor");
      p.u_zfar         = p.program.uniform("uZFar");
      p.u_fog_near     = p.program.uniform("uFogNear");
      p.u_fog_far      = p.program.uniform("uFogFar");
      p.u_alpha_test   = p.program.uniform("uAlphaTest");
      p.u_prev_projection = p.program.uniform("uPrevProjection");
      p.u_prev_modelview  = p.program.uniform("uPrevModelView");

      // Sampler bindings live in the program object, so once is enough.
      // tBase is on unit 0, the array texture on unit 1.
      p.program.use();
      glUniform1i(p.u_base, 0);
      glUniform1i(p.u_atlas, 1);

      // See WORLD_FAR. Depth reaches the G-buffer normalised, and the fog
      // distances are scaled to match in apply_.
      set_float(p.u_zfar, WORLD_FAR);

      glUseProgram(0);
  }

  /* One buffer means one cache. See apply_. */
  GLfloat shared_projection_[16] {};
  GLfloat shared_modelview_[16] {};

  /*! Upload whatever changed since this program last drew. */
  void apply_(ProgramState& p, const GLfloat* projection, const GLfloat* modelview,
              float texmode, float light)
  {
      // The cache has to follow the state, and with a uniform block the state
      // is one buffer shared by every program instead of a copy inside each.
      // Caching per program there would be wrong in a way that only shows on
      // the second program: A writes its matrix, B overwrites it, A draws
      // again, decides nothing changed since *it* last wrote, and renders with
      // B's matrix. Which is exactly what the world did -- geometry gone, sky
      // left standing.
      GLfloat* seen_projection = ublock_active() ? shared_projection_
                                                 : p.last_projection;
      GLfloat* seen_modelview  = ublock_active() ? shared_modelview_
                                                 : p.last_modelview;

      if (differs_(seen_projection, projection, 16)) {
          set_mat4(p.u_projection, projection);
          remember_(seen_projection, projection, 16);
      }

      if (differs_(seen_modelview, modelview, 16)) {
          set_mat4(p.u_modelview, modelview);
          remember_(seen_modelview, modelview, 16);
      }

      if (p.last_texmode != texmode) {
          glUniform1f(p.u_texmode, texmode);
          p.last_texmode = texmode;
      }

      if (p.u_sector_light >= 0 && p.last_sector_light != light) {
          glUniform3f(p.u_sector_light, light, light, light);
          p.last_sector_light = light;
      }

      if (differs_(p.last_fog_params, fog_params_, 3)) {
          glUniform3f(p.u_fog_params, fog_params_[0], fog_params_[1], fog_params_[2]);
          remember_(p.last_fog_params, fog_params_, 3);
      }

      // doomSceneMain takes near and far separately and smoothsteps between
      // them, where the engine mixes linearly. GL_EXP has no equivalent, so it
      // is approximated by the distance at which the exponential has faded.
      if (p.u_fog_near != -1) {
          float near = fog_params_[1];
          float far = fog_params_[2];

          if (fog_params_[0] < 0.5f) {
              // Fog off: push it past anything the level can reach. Checked
              // first -- the world distances below are the level's, and say
              // nothing about whether fog is running at all.
              near = 1.0e9f;
              far = 1.0e9f;
          } else if (world_fog_far_ > 0.0f) {
              // What the original actually says. See set_world_fog.
              near = world_fog_near_;
              far = world_fog_far_;
          } else if (fog_params_[0] > 1.5f) {
              near = 0.0f;
              far = 3.0f / std::max(fog_params_[1], 0.0001f);
          }

          // Into the same normalised space as uZFar, so the comparison the
          // shader makes against depth still means the same thing.
          set_float(p.u_fog_near, near / WORLD_FAR);
          set_float(p.u_fog_far, far / WORLD_FAR);
      }

      // GL_ALPHA_TEST is gone in a core profile, so the shader does it. The
      // engine only ever uses GL_GEQUAL, which is what the discard reproduces.
      float alpha[2] = {
          state_alpha_test_enabled() ? 1.0f : 0.0f,
          state_alpha_test_ref()
      };

      if (differs_(p.last_alpha_test, alpha, 2)) {
          glUniform2f(p.u_alpha_test, alpha[0], alpha[1]);
          remember_(p.last_alpha_test, alpha, 2);
      }

      // Uploaded every time rather than cached: they change once a frame, and
      // the cache would have to be invalidated on exactly that boundary.
      // != -1, not >= 0: a block member reports an encoded negative location,
      // and -1 is the only value that means "this shader has no such uniform".
      if (p.u_prev_projection != -1) {
          set_mat4(p.u_prev_projection, have_prev_ ? prev_projection_ : projection);
      }

      if (p.u_prev_modelview != -1) {
          set_mat4(p.u_prev_modelview, have_prev_ ? prev_modelview_ : modelview);
      }

      if (differs_(p.last_fog_color, fog_color_, 3)) {
          set_vec3(p.u_fog_color, fog_color_);
          remember_(p.last_fog_color, fog_color_, 3);
      }
  }
  /* Streaming ring buffers.
   *
   * Writing every batch to offset 0 makes the driver wait for the previous draw
   * to finish reading before it may overwrite. Respecifying the store instead
   * (glBufferData per batch) trades that stall for an allocation, which is no
   * better -- it was worth 44 FPS when measured.
   *
   * So: one large buffer, written at an advancing offset, orphaned only when it
   * wraps. */
  constexpr size_t VBO_BYTES = 2u * 1024u * 1024u;

  constexpr size_t IBO_BYTES = 512u * 1024u;

  size_t vbo_offset_ {};
  size_t ibo_offset_ {};

  size_t stream_(GLenum target, size_t& offset, size_t capacity,
                 const void* data, size_t bytes)
  {
      if (offset + bytes > capacity) {
          // Orphan on wrap: the driver hands back fresh storage rather than
          // waiting on whatever is still reading the old one.
          glBufferData(target, static_cast<gl33::sizeiptr>(capacity),
                             nullptr, GL_STREAM_DRAW);
          offset = 0;
      }

      size_t at = offset;
      glBufferSubData(target, static_cast<gl33::intptr>(at),
                            static_cast<gl33::sizeiptr>(bytes), data);

      // Keep the next batch aligned; vtx_t is 40 bytes, so offsets would
      // otherwise drift off a natural boundary.
      offset = at + ((bytes + 31u) & ~31u);
      return at;
  }


}

void shader::configure_preprocessor(Preprocessor& pp)
{
    // KEX's own escape hatch: this turns every cbuffer member into a plain
    // uniform, so there is no std140 block to lay out by hand. A real RenderView
    // buffer can replace this once more than two matrices are actually needed.
    // KEX gl_useUniformBuffers. With it off -- the default -- every cbuffer
    // member becomes a plain uniform and there is no std140 block to lay out.
    // With it on, globals_structs.inc declares a real RenderView block and
    // uniformblock.cc owns the buffer behind it.
    ublock_decide();

    if (!ublock_active())
        pp.define("EMULATE_UNIFORM_BUFFERS");

    // Adapts the KEX shaders in place of editing them: see
    // progs/d64ex/overrides.inc.
    pp.inject_after("progs/common.inc", "progs/d64ex/overrides.inc");
}

void shader::init_draw()
{
    if (!gl33::loaded())
        return;

    try {
        Preprocessor pp;
        configure_preprocessor(pp);

        generic_.program = Program::build("progs/d64ex/generic.shader", pp);

        // KEX's own world shader, unmodified, through both of its entry points:
        // doomSceneNoFilter leaves USE_FILTERING off, doomSceneFilter turns on
        // the N64 three-point filter. r_N64Filter picks between them per draw.
        world_.program = Program::build("progs/doomSceneNoFilter.shader", pp);
        world_filtered_.program = Program::build("progs/doomSceneFilter.shader", pp);

        // And again as a G-buffer. HAS_MRT is KEX's own switch; the three
        // fragment outputs have to be declared here because common_glsl.inc
        // leaves def_var_pixelTarget empty outside Vulkan.
        Preprocessor mrt;
        configure_preprocessor(mrt);
        mrt.define("HAS_MRT", 1);
        mrt.fragment_outputs(3);

        world_mrt_.program = Program::build("progs/doomSceneNoFilter.shader", mrt);
        world_filtered_mrt_.program = Program::build("progs/doomSceneFilter.shader", mrt);

        // Unconditional now: there is no second path left to fall back to, so
        // the array is always what the world is drawn from.
        atlas_build();
    } catch (const std::exception& e) {
        log::warn("Programmable path unavailable: {}", e.what());
        return;
    }

    locate_(generic_);
    locate_(world_);
    locate_(world_filtered_);
    locate_(world_mrt_);
    locate_(world_filtered_mrt_);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ibo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<gl33::sizeiptr>(VBO_BYTES),
                       nullptr, GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<gl33::sizeiptr>(IBO_BYTES),
                       nullptr, GL_STREAM_DRAW);

    // Matches common.inc: 0 position, 1 texcoord, 2 colour.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vtx_t),
                                reinterpret_cast<void*>(offsetof(vtx_t, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vtx_t),
                                reinterpret_cast<void*>(offsetof(vtx_t, tu)));

    // Normalised, exactly as glColorPointer treats unsigned bytes.
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx_t),
                                reinterpret_cast<void*>(offsetof(vtx_t, r)));

    // Attributes 3 to 6 carry the atlas metadata, in the layout
    // progs/doomSceneMain.shader expects. The first three are integers and must
    // go through glVertexAttribIPointer, not the float path.
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(vtx_t),
                                 reinterpret_cast<void*>(offsetof(vtx_t, offset)));

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 1, GL_INT, sizeof(vtx_t),
                                 reinterpret_cast<void*>(offsetof(vtx_t, dimensions)));

    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 1, GL_INT, sizeof(vtx_t),
                                 reinterpret_cast<void*>(offsetof(vtx_t, options)));

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx_t),
                                reinterpret_cast<void*>(offsetof(vtx_t, fr)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    ready_ = true;

    log::info("Programmable path ready: {} + {} + {}",
              world_.program.path(), world_filtered_.program.path(),
              generic_.program.path());
}

bool shader::draw_ready()
{
    return ready_;
}

void shader::set_sector_light(int params)
{
    // DL_ProcessDrawList builds a 1x1 texture of (l, l, l) with l = params >> 1
    // and has the second unit GL_ADD it. Same value, same scale.
    sector_light_ = static_cast<float>(params >> 1) / 255.0f;
}

void shader::stamp_batch(vtx_t* verts, size_t count,
                         unsigned offset, unsigned layer,
                         unsigned width, unsigned height,
                         bool mirror_s, bool mirror_t, unsigned glow)
{
    // Layout taken from progs/doomSceneMain.shader: bit 0 flips vertically,
    // bit 1 horizontally, and the sector light rides in bits 3 to 10.
    int dims = static_cast<int>((width << 16) | (height & 0xFFFF));
    int opts = (mirror_t ? 1 : 0)
             | (mirror_s ? 2 : 0)
             | static_cast<int>((glow & 0xFF) << 3);

    for (size_t i = 0; i < count; ++i) {
        verts[i].offset = static_cast<int>(offset);
        verts[i].dimensions = dims;
        verts[i].options = opts;
        verts[i].fr = 0;
        verts[i].fg = 0;
        verts[i].fb = 0;
        verts[i].layer = static_cast<byte>(layer);
    }

    atlas_batch_ = true;
}

void shader::draw_geometry(const vtx_t* verts, size_t vert_count,
                           const unsigned short* indices, size_t index_count,
                           unsigned mode)
{
    if (!ready_ || !vert_count || !index_count)
        return;

    // The fixed pipeline leaves errors of its own behind; drain them so the
    // probe after the draw only reports on this path.
    if (!probed_) {
        while (dglGetError() != GL_NO_ERROR)
            ;
    }

    // From the engine's own stack, not glGetFloatv. The core profile has no
    // matrix stack to read, and matrix_selftest proved this one equivalent over
    // 20000 draws -- so the shaders take it directly and the last per-draw
    // dependency on fixed-function state goes away.
    const GLfloat* projection = matrix_get(MatrixKind::projection);
    const GLfloat* modelview = matrix_get(MatrixKind::modelview);

    // Opt-in: still worth checking while the fixed stack exists to check against.
    if (M_CheckParm("-mtxcheck")) {
        matrix_selftest();
        state_selftest();

    }

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    size_t vbase = stream_(GL_ARRAY_BUFFER, vbo_offset_, VBO_BYTES,
                           verts, vert_count * sizeof(vtx_t));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    size_t ibase = stream_(GL_ELEMENT_ARRAY_BUFFER, ibo_offset_, IBO_BYTES,
                           indices, index_count * sizeof(unsigned short));

    // The attribute pointers carry the batch's base offset, so they move with it.
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vtx_t),
                                reinterpret_cast<void*>(vbase + offsetof(vtx_t, x)));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vtx_t),
                                reinterpret_cast<void*>(vbase + offsetof(vtx_t, tu)));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx_t),
                                reinterpret_cast<void*>(vbase + offsetof(vtx_t, r)));

    // Attributes 3 to 6 carry the atlas metadata, in the layout
    // progs/doomSceneMain.shader expects. The first three are integers and must
    // go through glVertexAttribIPointer, not the float path.
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(vtx_t),
                                 reinterpret_cast<void*>(vbase + offsetof(vtx_t, offset)));

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, 1, GL_INT, sizeof(vtx_t),
                                 reinterpret_cast<void*>(vbase + offsetof(vtx_t, dimensions)));

    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 1, GL_INT, sizeof(vtx_t),
                                 reinterpret_cast<void*>(vbase + offsetof(vtx_t, options)));

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vtx_t),
                                reinterpret_cast<void*>(vbase + offsetof(vtx_t, fr)));

    // Fog, from the engine's own mirror rather than glGet. SetupFog is still
    // the only thing that decides what the fog is; this just reads it back from
    // somewhere the core profile can answer.
    if (state_fog_enabled()) {
        if (!fog_cached_) {
            if (state_fog_mode() == GL_EXP) {
                fog_params_[0] = 2.0f;
                fog_params_[1] = state_fog_density();
                fog_params_[2] = 0.0f;
            } else {
                fog_params_[0] = 1.0f;
                fog_params_[1] = state_fog_start();
                fog_params_[2] = state_fog_end();
            }

            const float* c = state_fog_color();
            fog_color_[0] = c[0];
            fog_color_[1] = c[1];
            fog_color_[2] = c[2];

            fog_cached_ = true;
        }
    }
    else {
        fog_params_[0] = fog_params_[1] = fog_params_[2] = 0.0f;
        fog_cached_ = false;
    }

    // World geometry goes through the program that reads the array texture;
    // everything else through the one that binds a 2D texture.
    bool use_world = atlas_batch_ && atlas_ready() && static_cast<bool>(world_);

    // Four combinations of two independent choices: the N64 filter, and
    // whether a G-buffer is being written this frame.
    bool filtered = *r_n64filter;
    bool mrt = gbuffer_wanted();

    ProgramState* pick = filtered ? &world_filtered_ : &world_;

    if (mrt) {
        ProgramState* g = filtered ? &world_filtered_mrt_ : &world_mrt_;
        if (*g)
            pick = g;
    }

    ProgramState& world = *pick;

    ProgramState& p = use_world ? world : generic_;

    p.program.use();

    if (use_world) {
        // GL_TEXTURE_2D and GL_TEXTURE_2D_ARRAY are separate binding points on
        // the same unit, so this never fights with the engine's own textures.
        dglBindTexture(GL_TEXTURE_2D_ARRAY, atlas_texture());
    }

    // Mirror unit 0's texture environment: the melt uses GL_ADD, some paths
    // GL_REPLACE. Assuming GL_MODULATE turns the screen melt black.
    float texmode = 0.0f;
    if (!state_texture_enabled(0)) {
        // The immediate-mode paths turn texturing off and rely on the vertex
        // colour alone; the world path always has a texture.
        texmode = 3.0f;
    } else {
        switch (GL_GetTextureMode(0)) {
        case GL_ADD:     texmode = 1.0f; break;
        case GL_REPLACE: texmode = 2.0f; break;
        default:         texmode = 0.0f; break;
        }
    }

    apply_(p, projection, modelview, texmode, sector_light_);

    if (use_world) {
        remember_(last_projection_, projection, 16);
        remember_(last_modelview_, modelview, 16);
    }

    // -shaderalpha takes GL_ALPHA_TEST out of the picture so the shader's own
    // discard is the only thing rejecting fragments. Raw glDisable, not dgl, so
    // the state mirror keeps telling the shader what the engine asked for.
    static int shader_alpha = -1;
    if (shader_alpha < 0)
        shader_alpha = M_CheckParm("-shaderalpha") ? 1 : 0;

    bool masked = (shader_alpha && state_alpha_test_enabled());
    if (masked)
        glDisable(GL_ALPHA_TEST);

    dglDrawElements(mode, static_cast<GLsizei>(index_count),
                    GL_UNSIGNED_SHORT, reinterpret_cast<void*>(ibase));

    if (masked)
        glEnable(GL_ALPHA_TEST);

    // Report once. A misconfigured attribute or an unbound buffer shows up here
    // as GL_INVALID_OPERATION, which is otherwise silent -- the screen just
    // stays empty.
    if (!probed_) {
        probed_ = true;

        GLenum err = dglGetError();
        if (err != GL_NO_ERROR) {
            log::warn("First programmable draw raised GL error 0x{:x}",
                      static_cast<unsigned>(err));
        } else {
            log::info("First programmable draw: {} vertices, {} indices, no GL error",
                      vert_count, index_count);
        }
    }

    // Consumed. Anything drawn without DL_ProcessDrawList in front of it -- the
    // HUD, the console, the menus -- must not inherit the last sector's light,
    // nor be read as if it had atlas metadata.
    sector_light_ = 0.0f;
    atlas_batch_ = false;

    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void shader::world_frame_done()
{
    // Called once the world is finished, so what it was drawn with becomes what
    // the next frame calls "previous". Anything later in the frame -- the HUD,
    // the menus -- uses the generic program and never touches these.
    remember_(prev_projection_, last_projection_, 16);
    remember_(prev_modelview_, last_modelview_, 16);
    have_prev_ = true;
}

const float* shader::world_projection()
{
    // SAO needs the focal lengths to turn a depth back into an eye position,
    // and they are the two diagonal terms of this. Nothing else in the engine
    // keeps the world's projection once the frame has moved on.
    return last_projection_;
}

float shader::world_far()
{
    return WORLD_FAR;
}

void shader::set_world_fog(float near_units, float far_units)
{
    world_fog_near_ = near_units;
    world_fog_far_ = far_units;
}
