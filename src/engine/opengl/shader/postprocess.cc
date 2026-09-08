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
#include "doomstat.h"
#include "core/cvar.hh"

#include "shader/postprocess.hh"
#include "shader/program.hh"
#include "shader/draw.hh"
#include "shader/gl33.hh"
#include "shader/atlas.hh"
#include "shader/uniformblock.hh"

// GL_TEXTURE_2D_ARRAY, absent from the 1.4 glad header. Same as atlas.cc.
#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif
#include "wad/wad.hh"
#include "gl_main.h"
#include "g_actions.h"
#include "SDL.h"

using namespace imp;
using namespace imp::shader;

/*
 * Write doomSceneMain's three-target G-buffer: colour, depth, packed
 * velocity.
 *
 * Nothing consumes it yet -- it is what SAO and the motion blur will read.
 * r_GBufferShow displays one of the targets so it can be checked before
 * anything depends on it: 0 normal, 1 depth, 2 velocity, 3 the mask.
 */
cvar::IntVar r_gbuffer = 0;
cvar::IntVar r_gbuffer_show = 0;

/*
 * The three debug views KEX keeps as cvars of their own.
 *
 * Two of them we already had, folded into the level-2 rung of the effect they
 * belong to -- r_SAO 2 and r_MotionBlur 2. That is convenient while working on
 * the effect and useless to anyone reading their cvar list looking for the
 * switch by name, so each also gets the name they use. Either route turns the
 * view on; neither can turn the other off.
 *
 * r_ShowVirtualVRAM is new. It displays one layer of the texture array that
 * stands in for the N64's TMEM -- their r_vram.cpp, our atlas.cc -- through
 * progs/doomDisplayTexArray.shader, which is their own viewer and has been
 * sitting unused in the pk3. 0 is off, 1 shows the first layer, and the value
 * is clamped to however many the atlas built.
 */
cvar::BoolVar r_visualizeao = false;
cvar::BoolVar r_motionblurvisualize = false;
cvar::IntVar r_showvirtualvram = 0;

/*
 * Per-object motion blur, KEX's four-pass chain.
 *
 * 0 off, 1 on, 2 on with the tile visualisation drawn over it. It needs the
 * G-buffer, so turning it on turns that on too.
 */
cvar::IntVar r_motionblur = 0;
cvar::FloatVar r_motionblur_scale = 1.0f;

/*
 * Scalable Ambient Obscurance: the darkening in creases and corners.
 *
 * 0 off, 1 on, 2 shows the raw occlusion instead of the scene. It reads the
 * G-buffer depth, so it turns that on by itself.
 *
 * r_MaxOcclusionUnit is KEX's own name for the world radius the sampling disk
 * covers, and 0.01 is the value their config ships with.
 */
cvar::IntVar r_sao = 0;
cvar::FloatVar r_sao_radius = 0.01f;
cvar::FloatVar r_sao_intensity = 1.0f;

/*
 * Subpixel Morphological Anti-Aliasing: KEX three passes, unmodified.
 *
 * 0 off, 1 on, 2 shows the edges the first pass found, 3 the blend weights
 * the second one computed. Those two views are the only way to tell a pass
 * that found nothing from one that found the wrong thing.
 *
 * It costs more than FXAA and keeps more detail. Where both are asked for,
 * SMAA wins -- running two anti-aliasers over each other only smears.
 */
/*
 * The one anti-aliasing control, replacing r_FXAA and r_SMAA.
 *
 * KEX has a single r_antialiasing, "Sets a antialiasing mode", and the shader
 * paths sitting beside it in the binary give the order: fxaa_fast, fxaa, then
 * the three SMAA passes.
 *
 *   0  off
 *   1  FXAA
 *   2  FXAA, fast variant
 *   3  SMAA
 *
 * Two cvars for one decision was always wrong: nothing sensible happens when
 * both are on, so the old code had to pick a winner behind the player's back.
 * A mode cannot contradict itself.
 *
 * The two SMAA debug views moved out to r_SMAAShow rather than becoming modes 4
 * and 5. A debug view is not an anti-aliasing setting, and r_GBufferShow had
 * already established where those belong.
 */
cvar::IntVar r_antialiasing = 0;
cvar::IntVar r_smaa_show = 0;

/*
 * Radius, in pixels, of the blur laid over the scene when the interface asks
 * for it through post_scene_blur. 0 turns it off outright.
 *
 * progs/simpleBlur.shader, KEX's kexRenderPostProcessBlur. Their engine has no
 * cvar for it -- it is simply on -- but the cost is quadratic in the radius, so
 * here it is worth being able to say how far it reaches.
 *
 * It is not an effect on the scene: the scene has not changed. It pushes the
 * whole finished picture back so that the menu in front of it reads, which is
 * why it runs after everything else and why the HUD, drawn later and straight
 * to the window, is left sharp.
 */
cvar::IntVar r_menublur = 8;

/*
 * Fraction of the window the scene is rendered at. 1 is native.
 *
 * KEX's r_resolutionscale. The scene goes into a smaller target, the whole
 * post-process chain runs at that size, and one final pass stretches the result
 * to the window. The interface is drawn afterwards, straight to the window, so
 * the HUD, the automap, the console and the menus stay at full resolution
 * whatever this is set to -- which is the entire point of doing it here rather
 * than by resizing the window.
 *
 * Clamped to [0.25, 1]: below a quarter there is nothing left to look at.
 */
cvar::FloatVar r_resolutionscale = 1.0f;

/*
 * The dynamic half of KEX's r_resolutionscale, with their names and their
 * defaults, read out of the binary's string pool.
 *
 * Their r_resolutionscale is an enable and r_resolutionscale_fixedscale carries
 * the fraction. Ours has shipped as the fraction since the sixth, so it stays
 * the fraction and the controller gets its own switch. One deviation, stated,
 * rather than a cvar that silently changes meaning under someone's config.
 *
 * The two draw-time numbers are fractions of the frame budget, not
 * milliseconds. Their own stat page says so: the "resolutionscale" group prints
 * a column called "Time Fraction" beside "Active Scale". It also has to be that
 * way to be dimensionally sensible -- a renderer that had to finish in 1.125 ms
 * would be a renderer that never scaled at all.
 *
 * 20 frames before dropping against 200 before rising is not a typo of theirs:
 * fall in a third of a second, climb back over three. Hysteresis, so that a
 * scale which just fixed a stall does not immediately undo itself and start
 * oscillating -- which is the failure mode every dynamic resolution controller
 * has, and the reason the two counters exist at all.
 *
 * increasespeed had no default string next to it in the pool. 0.02 is ours: a
 * fifth of the drop step, which keeps the climb gentle in the same spirit as
 * the 200-frame wait.
 */
cvar::BoolVar r_resolutionscale_dynamic = false;
cvar::FloatVar r_resolutionscale_targetdrawtime = 1.125f;
cvar::FloatVar r_resolutionscale_gooddrawtime = 0.9f;
cvar::FloatVar r_resolutionscale_increasespeed = 0.02f;
cvar::FloatVar r_resolutionscale_lowerspeed = 0.1f;
cvar::BoolVar r_resolutionscale_aggressive = false;
cvar::IntVar r_resolutionscale_numframesbeforelowering = 20;
cvar::IntVar r_resolutionscale_numframesbeforeraising = 200;

/*
 * Environmental brightness: multiplies the finished world image.
 *
 * KEX's second brightness slider, `r_brightness`. Their menu has two, and the
 * one we already had -- i_Brightness -- is the other: it scales the sector
 * lights, exactly as the N64 does, and its range is the original's own
 * (factor 1.0 to 2.0). This one sits after everything, on the image, and so it
 * lifts the fog colour too. That is what the comparisons against KEX kept
 * showing and what neither the sector lights nor the fog could explain.
 *
 * 1 is neutral and is the default, so fidelity to the original costs nothing:
 * measured, KEX's whole range sits above the original's, and leaving this alone
 * keeps us on the original's.
 *
 * Only the world is affected. The HUD, the automap, the console and the menus
 * are drawn afterwards, straight to the window.
 */
cvar::FloatVar r_brightness = 1.0f;

/* The engine's gamma, now consumed here instead of by the palette loader. */
extern cvar::FloatVar i_gamma;

namespace {
  /* The scene, and the depth to render it with. Colour is a texture because
   * the post shader has to sample it; depth is a renderbuffer because nothing
   * reads it yet. It becomes a texture the day SAO needs it. */
  GLuint fbo_ {};
  GLuint color_ {};
  GLuint depth_ {};

  /* The two extra G-buffer targets. Half float rather than RGBA8: the depth
   * target holds a normalised distance that would band badly in 8 bits, and
   * the velocity one carries a signed value folded into [0,1]. */
  GLuint gdepth_ {};
  GLuint gvelocity_ {};

  /* Whether the targets currently attached include the G-buffer pair. */
  bool targets_mrt_ {};
  bool active_mrt_ {};

  /* The size the scene is rendered at. Equal to the window unless
   * r_ResolutionScale asks for less. Every off-screen target and every pass
   * that writes to one is sized from these. */
  int width_ {};
  int height_ {};

  /* The window. Only the very last pass, the one that puts the image on
   * screen, uses these -- and when the two pairs differ, that pass is the
   * stretch. */
  int out_width_ {};
  int out_height_ {};

  /* What the screen-size globals held before post_begin borrowed them.
   *
   * The whole world path -- GL_ClearView, GL_ResetViewport, the sky, the
   * sprites -- takes its viewport from ViewWidth and friends and from nothing
   * else, so pointing them at the reduced target is all it takes for the scene
   * to be rendered smaller.
   *
   * video_width and video_height go with them, and that is not optional:
   * GL_Set2DQuad places the player's weapon with
   *
   *     left = ViewWindowX + x * ViewWidth / video_width;
   *
   * a ratio that is 1 whenever the view fills the screen. Borrowing only the
   * first pair leaves it at the scale factor, and the weapon comes out halved a
   * second time, adrift in a corner. While the target is bound the target is
   * the screen, so both pairs describe it.
   *
   * Checked: inside the world pass nothing else reads either pair except
   * dglViewFrustum, which takes only their ratio. */
  int saved_view_[6] {};
  bool view_borrowed_ {};

  /* One quad, in clip space. The post shaders still multiply by the two
   * matrices, so both are handed identity rather than the shader being
   * changed. */
  GLuint vao_ {};
  GLuint vbo_ {};

  /* How many pixels of the screen one velocity tile covers. The blur can
   * never reach further than this, and the two tile passes reduce the
   * velocity field by exactly this factor on each axis. */
  constexpr int TILE = 20;

  /*!
   * A colour target with its own framebuffer.
   *
   * The motion blur chain is four passes across three intermediate images of
   * three different sizes, so they are worth a type rather than twelve more
   * globals.
   */
  struct Target {
      GLuint fbo {};
      GLuint tex {};
      int w {};
      int h {};

      void destroy()
      {
          if (tex) {
              dglDeleteTextures(1, &tex);
              tex = 0;
          }

          if (fbo) {
              glDeleteFramebuffers(1, &fbo);
              fbo = 0;
          }

          w = h = 0;
      }

      /*! Resize if needed. Reports whether it is usable. */
      bool ensure(int width, int height)
      {
          if (fbo && w == width && h == height)
              return true;

          destroy();

          if (width <= 0 || height <= 0)
              return false;

          w = width;
          h = height;

          dglGenTextures(1, &tex);
          dglBindTexture(GL_TEXTURE_2D, tex);
          dglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
          dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, DGL_CLAMP);
          dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, DGL_CLAMP);
          dglBindTexture(GL_TEXTURE_2D, 0);

          glGenFramebuffers(1, &fbo);
          glBindFramebuffer(GL_FRAMEBUFFER, fbo);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, tex, 0);

          GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
          glBindFramebuffer(GL_FRAMEBUFFER, 0);

          if (status != GL_FRAMEBUFFER_COMPLETE) {
              log::warn("Motion blur target {}x{} incomplete (0x{:x})",
                        w, h, static_cast<unsigned>(status));
              destroy();
              return false;
          }

          return true;
      }
  };

  /* Full resolution: velocity encoded to 8 bits, with its length and the
   * depth packed alongside. This is what the blur pass reads per pixel. */
  Target encode_;

  /* The velocity field reduced to tiles, one axis at a time, then spread to
   * its neighbours so a fast object bleeds into the tiles around it. */
  Target tile_h_;
  Target tile_v_;
  Target tile_n_;

  /* SAO's depth pyramid. One R32F texture with five levels, and one
   * framebuffer per level so each can be rendered into. sao.shader picks a
   * level from the sampling radius: past sixteen pixels it drops down the
   * pyramid rather than scattering reads across a full-resolution image. */
  static constexpr int SAO_MIPS = 5;

  GLuint sao_depth_ {};
  GLuint sao_depth_fbo_[SAO_MIPS] {};
  int sao_depth_w_ {};
  int sao_depth_h_ {};

  /* The occlusion itself, and the two halves of the bilateral blur that
   * cleans it up. The blur is separable, so it ping-pongs. */
  Target sao_ao_;
  Target sao_blur_;

  /* SMAA. The two reference textures are not in the kpf: Nightdive compile
   * theirs into the executable as C++ classes, so these come from the
   * official SMAA distribution instead. See progs/d64ex/SMAA-LICENSE.txt. */
  GLuint smaa_area_ {};
  GLuint smaa_search_ {};
  bool smaa_tex_tried_ {};

  /* What each of the first two passes writes. Both must be cleared every
   * frame: the shaders discard where they find nothing, so anything left
   * behind would be read as a real edge. */
  Target smaa_edges_;
  Target smaa_blend_;

  /* Where the motion blur puts the scene when SMAA follows it, since the
   * anti-aliaser needs an image to sample rather than a window. */
  Target scene_;

  /* The finished image, when something still has to be done to it -- a menu
   * blur, a stretch to the window, or both. Same reason as scene_: those read a
   * texture, and the window is not one. Distinct from scene_ because the motion
   * blur may already be using that one. */
  Target menu_blur_src_;

  /* And where the menu blur puts its result when the stretch follows it, since
   * two stages in a row cannot share one target. */
  Target menu_blur_dst_;

  struct PostProgram {
      Program program;

      int u_projection { -1 };
      int u_modelview { -1 };
      int u_base { -1 };
      int u_view_width { -1 };
      int u_view_height { -1 };
      int u_show_mode { -1 };
      int u_params { -1 };
      int u_encode { -1 };
      int u_velocity { -1 };

      // SAO and the bilateral blur.
      int u_params1 { -1 };
      int u_params2 { -1 };
      int u_texture_sizes { -1 };
      int u_inv_focal { -1 };
      int u_zfar { -1 };
      int u_image_size { -1 };
      int u_mip_index { -1 };
      int u_strides { -1 };
      int u_gauss { -1 };
      int u_texture_size { -1 };

      // SMAA: vec4(1/w, 1/h, w, h). The shaders offset texcoords with the
      // first pair and recover a pixel coordinate with the second.
      int u_rt_size { -1 };

      // simpleBlur's only uniform. Plain uParams, distinct from the numbered
      // uParams1/uParams2 that SAO uses.
      int u_blur_params { -1 };

      // gbuffer.shader only: the environmental brightness multiply.
      int u_env_brightness { -1 };
      int u_gamma { -1 };

      explicit operator bool() const
      { return static_cast<bool>(program); }
  };

  PostProgram fxaa_;
  PostProgram fxaa_fast_;
  PostProgram blit_;

  /* progs/motionBlurMain.shader, through four of its five entry points. */
  PostProgram mb_pack_;
  PostProgram mb_tile_;
  PostProgram mb_neighbour_;
  PostProgram mb_apply_;
  PostProgram mb_show_;

  /* SAO, and the two entry points of progs/bilateralBlurMain.shader. */
  PostProgram sao_copy_;
  PostProgram sao_down_;
  PostProgram sao_;
  PostProgram blur_h_;
  PostProgram blur_v_;

  /* SMAA. Three separate shaders, not variants of one body. */
  PostProgram smaa_edge_;
  PostProgram smaa_weights_;
  PostProgram smaa_neighbour_;

  /* progs/simpleBlur.shader, over the finished image when the interface asks. */
  PostProgram blur_simple_;

  /* progs/doomDisplayTexArray.shader -- their viewer for the virtual VRAM. */
  PostProgram vram_;

  /* What post_scene_blur was last told, 0..1. A level, not a pulse: M_Ticker
   * writes it every tic, including the zero that turns it off, so nothing here
   * has to guess when it went stale. Zero until something asks, which is what
   * keeps the renderer out of the business of reading game state. */
  float scene_blur_ {};

  bool ready_ {};
  bool bound_ {};
  bool warned_ {};

  const float identity_[16] = {
      1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f
  };

  void locate_(PostProgram& p)
  {
      if (!p)
          return;

      // Attach it to the shared RenderView buffer before anything asks for a
      // location: ublock_init learns the member offsets from the first program
      // that carries the block, and every lookup after that depends on them.
      ublock_bind_program(p.program);

      p.u_projection  = p.program.uniform("uProjectionMatrix");
      p.u_modelview   = p.program.uniform("uModelViewMatrix");
      p.u_base        = p.program.uniform("uTex_tBase");
      p.u_view_width  = p.program.uniform("uViewWidth");
      p.u_view_height = p.program.uniform("uViewHeight");
      p.u_show_mode   = p.program.uniform("uShowMode");
      p.u_params      = p.program.uniform("uMotionBlurParams");
      p.u_encode      = p.program.uniform("uTex_tEncode");
      p.u_velocity    = p.program.uniform("uTex_tVelocity");
      p.u_params1     = p.program.uniform("uParams1");
      p.u_params2     = p.program.uniform("uParams2");
      p.u_texture_sizes = p.program.uniform("uTextureSizes");
      p.u_inv_focal   = p.program.uniform("uInvFocalCoords");
      p.u_zfar        = p.program.uniform("uZFar");
      p.u_image_size  = p.program.uniform("uImageSize");
      p.u_mip_index   = p.program.uniform("uMipIndex");
      p.u_strides     = p.program.uniform("uStrides");
      p.u_gauss       = p.program.uniform("uGaussWeights");
      p.u_texture_size = p.program.uniform("uTextureSize");
      p.u_rt_size      = p.program.uniform("uRTSize");
      p.u_blur_params  = p.program.uniform("uParams");
      p.u_env_brightness = p.program.uniform("uEnvBrightness");
      p.u_gamma          = p.program.uniform("uGamma");

      p.program.use();
      glUniform1i(p.u_base, 0);
      // tDepth is on unit 1 for the G-buffer view, but SAO and the depth
      // pyramid are the only thing they read, so it goes on unit 0 there.
      glUniform1i(p.program.uniform("uTex_tDepth"),
                        (&p == &sao_ || &p == &sao_copy_ || &p == &sao_down_) ? 0 : 1);
      glUniform1i(p.program.uniform("uTex_tTexture"), 0);

      // motionBlurMain moves tVelocity between units depending on the entry
      // point: unit 0 for every pre-pass, unit 2 for the blur itself, where
      // units 0 and 1 are the scene and the packed velocity.
      glUniform1i(p.u_encode, 1);
      glUniform1i(p.u_velocity,
                        (&p == &mb_apply_ || &p == &blit_) ? 2 : 0);

      // SMAA. The three passes name their inputs differently but the units
      // are consistent: the image being worked on is 0, and whatever it is
      // looked up against follows. A program without one of these gets a
      // location of -1, and glUniform1i on -1 does nothing.
      glUniform1i(p.program.uniform("uTex_tEdgeRenderTarget"), 0);
      glUniform1i(p.program.uniform("uTex_tDest"), 0);
      glUniform1i(p.program.uniform("uTex_tArea"), 1);
      glUniform1i(p.program.uniform("uTex_tBlendWeights"), 1);
      glUniform1i(p.program.uniform("uTex_tSearch"), 2);

      glUseProgram(0);
  }

  /*! Tear down the depth pyramid. */
  void destroy_sao_depth_()
  {
      for (int i = 0; i < SAO_MIPS; ++i) {
          if (sao_depth_fbo_[i]) {
              glDeleteFramebuffers(1, &sao_depth_fbo_[i]);
              sao_depth_fbo_[i] = 0;
          }
      }

      if (sao_depth_) {
          dglDeleteTextures(1, &sao_depth_);
          sao_depth_ = 0;
      }

      sao_depth_w_ = sao_depth_h_ = 0;
  }

  /*!
   * One R32F texture with five levels, each with its own framebuffer.
   *
   * The levels are allocated by hand rather than by glGenerateMipmap: they are
   * rendered into, one pass per level, and a generated chain would be
   * overwritten anyway.
   */
  bool ensure_sao_depth_()
  {
      if (sao_depth_ && sao_depth_w_ == width_ && sao_depth_h_ == height_)
          return true;

      destroy_sao_depth_();

      if (width_ <= 0 || height_ <= 0)
          return false;

      sao_depth_w_ = width_;
      sao_depth_h_ = height_;

      dglGenTextures(1, &sao_depth_);
      dglBindTexture(GL_TEXTURE_2D, sao_depth_);

      for (int i = 0; i < SAO_MIPS; ++i) {
          int w = width_ >> i;
          int h = height_ >> i;

          if (w < 1) w = 1;
          if (h < 1) h = 1;

          dglTexImage2D(GL_TEXTURE_2D, i, GL_R32F, w, h, 0,
                        GL_RED, GL_FLOAT, nullptr);
      }

      // Nearest, and never past the levels that exist: sao.shader fetches
      // texels by integer coordinate, so filtering would only blur the depth
      // it is about to compare against.
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, DGL_CLAMP);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, DGL_CLAMP);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, SAO_MIPS - 1);
      dglBindTexture(GL_TEXTURE_2D, 0);

      for (int i = 0; i < SAO_MIPS; ++i) {
          glGenFramebuffers(1, &sao_depth_fbo_[i]);
          glBindFramebuffer(GL_FRAMEBUFFER, sao_depth_fbo_[i]);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, sao_depth_, i);

          GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
          if (status != GL_FRAMEBUFFER_COMPLETE) {
              log::warn("SAO depth level {} incomplete (0x{:x})",
                        i, static_cast<unsigned>(status));
              glBindFramebuffer(GL_FRAMEBUFFER, 0);
              destroy_sao_depth_();
              return false;
          }
      }

      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      log::info("SAO depth pyramid: {}x{}, {} levels", width_, height_, SAO_MIPS);
      return true;
  }

  /*!
   * Upload one of SMAA reference textures from the pk3.
   *
   * They are rows of raw bytes with no header, so there is nothing to decode.
   * The dimensions are not guessed either: they are fixed by constants inside
   * SMAA_blendWeights.shader itself, and a mismatch there would be silent.
   */
  GLuint load_smaa_tex_(const char* path, int w, int h, int channels,
                        int internal, unsigned format, int filter)
  {
      auto lump = wad::open_path(path);

      if (!lump) {
          log::warn("SMAA: {} not found in the pk3", path);
          return 0;
      }

      String bytes = lump->read_bytes();
      size_t want = static_cast<size_t>(w) * static_cast<size_t>(h) * channels;

      if (bytes.size() != want) {
          log::warn("SMAA: {} is {} bytes, expected {}", path, bytes.size(), want);
          return 0;
      }

      GLuint id = 0;
      dglGenTextures(1, &id);
      dglBindTexture(GL_TEXTURE_2D, id);

      // Rows are packed byte by byte. The search texture is 64 wide with one
      // channel, so the default four-byte row alignment would misread it.
      dglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      dglTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format,
                    GL_UNSIGNED_BYTE, bytes.data());
      dglPixelStorei(GL_UNPACK_ALIGNMENT, 4);

      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, DGL_CLAMP);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, DGL_CLAMP);
      dglBindTexture(GL_TEXTURE_2D, 0);

      return id;
  }

  /*! Load both reference textures, once per session. */
  bool ensure_smaa_tex_()
  {
      if (smaa_area_ && smaa_search_)
          return true;

      if (smaa_tex_tried_)
          return false;

      smaa_tex_tried_ = true;

      // Linear for the area texture: the shader reads between texels on purpose,
      // that is what the precomputed areas are for. Nearest for the search one,
      // whose lookup is already biased to texel centres.
      smaa_area_ = load_smaa_tex_("progs/d64ex/smaa_area.raw", 160, 560, 2,
                                  GL_RG8, GL_RG, GL_LINEAR);
      smaa_search_ = load_smaa_tex_("progs/d64ex/smaa_search.raw", 64, 16, 1,
                                    GL_R8, GL_RED, GL_NEAREST);

      if (smaa_area_ && smaa_search_) {
          log::info("SMAA textures: area 160x560 RG8, search 64x16 R8");
          return true;
      }

      log::warn("SMAA disabled: reference textures unavailable");
      return false;
  }

  /*! Whether the SMAA chain should run this frame. */
  bool smaa_on()
  {
      if (*r_gbuffer && *r_gbuffer_show)
          return false;

      return *r_antialiasing == 3 && smaa_edge_ && smaa_weights_ && smaa_neighbour_;
  }

  /*! Whether the occlusion pass should run this frame. */
  bool sao_on()
  {
      if (*r_gbuffer && *r_gbuffer_show)
          return false;

      // r_VisualizeAO on its own turns the occlusion on: asking to see a thing
      // and getting a blank screen because a second cvar was off is not help.
      return (*r_sao || *r_visualizeao) && targets_mrt_ && sao_ && sao_copy_
             && sao_down_ && blur_h_ && blur_v_;
  }

  /*! Whether the motion blur chain should run this frame. */
  bool motion_blur_on()
  {
      // Not while a G-buffer target is being shown: the point of that view is
      // to see the target itself, unblurred.
      if (*r_gbuffer && *r_gbuffer_show)
          return false;

      // As with r_VisualizeAO: the switch that shows it also turns it on.
      return (*r_motionblur || *r_motionblurvisualize) && targets_mrt_
             && mb_pack_ && mb_tile_ && mb_neighbour_ && mb_apply_;
  }

  /*!
   * Tap radius for the scene blur this frame, in pixels. 0 means do not run.
   *
   * The requested amount scales the radius rather than fading the result over
   * the sharp image, which would cost a second pass and a blend. Below one the
   * pass has to be skipped outright: simpleBlur takes a single tap at radius
   * zero, weights it by sqrt(uParams.w) minus zero, and with uParams.w also
   * zero the weight is zero -- the shader then clamps the divisor to one and
   * returns black. A blur that fades to black instead of to sharp is worse than
   * no blur at all.
   */
  int scene_blur_radius()
  {
      if (*r_gbuffer && *r_gbuffer_show)
          return 0;

      if (*r_menublur <= 0 || scene_blur_ <= 0.0f || !blur_simple_)
          return 0;

      float amount = scene_blur_ > 1.0f ? 1.0f : scene_blur_;

      return static_cast<int>(static_cast<float>(*r_menublur) * amount);
  }

  /*! Whether the scene blur should run this frame. */
  bool menu_blur_on()
  {
      return scene_blur_radius() >= 1;
  }

  /*!
   * Whether the environmental brightness is asking for anything.
   *
   * At 1 it is a multiply by one, and the frame has no reason to detour through
   * the framebuffer for it.
   */
  bool env_brightness_on()
  {
      if (*r_gbuffer && *r_gbuffer_show)
          return false;

      float e = *r_brightness;

      return (e < 0.999f || e > 1.001f) && blit_;
  }

  /*! The gamma setting, clamped to what the menu offers. */
  float gamma_value()
  {
      float g = *i_gamma;

      if (g < 0.0f) g = 0.0f;
      if (g > 20.0f) g = 20.0f;

      return g;
  }

  /*!
   * Whether the gamma curve needs the framebuffer.
   *
   * Zero is the identity, and a frame with nothing else to do has no reason to
   * detour through a render target to multiply by one.
   */
  bool gamma_on()
  {
      if (*r_gbuffer && *r_gbuffer_show)
          return false;

      return gamma_value() > 0.001f && blit_;
  }

  /*! Whether the occlusion should be shown instead of applied. */
  bool visualize_ao()
  {
      return *r_sao > 1 || *r_visualizeao;
  }

  /*! Whether the velocity tile grid should be drawn over the blurred scene. */
  bool visualize_motion_blur()
  {
      return *r_motionblur > 1 || *r_motionblurvisualize;
  }

  /*! Bind a texture to a unit without disturbing the engine's idea of unit 0. */
  void bind_unit_(int unit, GLuint tex)
  {
      glActiveTexture(GL_TEXTURE0 + unit);
      dglBindTexture(GL_TEXTURE_2D, tex);
      glActiveTexture(GL_TEXTURE0);
  }

  /*!
   * The two uniforms gbuffer.shader applies to the finished world image.
   *
   * Its own function because two paths draw that shader -- pass_, and the
   * hand-written present at the end of post_end_body_ -- and only one of them
   * used to post these. An unset uniform is zero, and the first thing the
   * shader does with uEnvBrightness is multiply by it, so the world came out
   * black. Anything that can be forgotten in one place out of two eventually is.
   *
   * The program must already be current: a uniform belongs to whatever use()
   * last bound.
   */
  void post_image_uniforms_(PostProgram& p)
  {
      // 0 to 2, which is KEX's own range, with 1 neutral. The shader adds
      // (value - 1), so 0 is a full stop darker and 2 a full stop lighter.
      //
      // It used to be clamped to 0.05 at the bottom, because back when this
      // multiplied, a value of exactly zero turned the world black with no way
      // out but the console. Adding -1 does the same, but the menu slider can
      // always walk back up, and 0 is where their own slider ends. Matching
      // their range beats guarding against a console value nobody types.
      if (p.u_env_brightness >= 0) {
          float env = *r_brightness;

          if (env < 0.0f) env = 0.0f;
          if (env > 2.0f) env = 2.0f;

          glUniform1f(p.u_env_brightness, env);
      }

      // The engine's own gamma expression, moved out of the palette and on to
      // the finished image. See gbuffer.shader for why it works in 0-255.
      if (p.u_gamma >= 0)
          glUniform1f(p.u_gamma, 1.0f + 0.01f * gamma_value());
  }

  /*!
   * One full-screen pass.
   *
   * p       Program to run
   * dst     Target, or nullptr for the window
   * params  uMotionBlurParams, four floats
   */
  void pass_(PostProgram& p, Target* dst, const float* params,
             float show_mode = -1.0f)
  {
      // A pass writing to a target works at the scene's size; one writing to
      // the window works at the window's. The two differ only when
      // r_ResolutionScale is below 1, and in that case the chain is routed so
      // that the pass reaching the window is the stretch and nothing else.
      int w = dst ? dst->w : out_width_;
      int h = dst ? dst->h : out_height_;

      glBindFramebuffer(GL_FRAMEBUFFER, dst ? dst->fbo : 0);
      dglViewport(0, 0, w, h);

      p.program.use();

      set_mat4(p.u_projection, identity_);
      set_mat4(p.u_modelview, identity_);
      set_float(p.u_view_width, static_cast<float>(w));
      set_float(p.u_view_height, static_cast<float>(h));

      if (p.u_params >= 0 && params)
          glUniform4fv(p.u_params, 1, params);

      // Here, and not at the call site: a uniform belongs to the program that
      // is current, and use() is above. Setting it beforehand wrote it into
      // whichever program happened to be bound, which is how the occlusion
      // kept arriving on screen as raw red instead of grey.
      if (show_mode >= 0.0f && p.u_show_mode >= 0)
          glUniform1f(p.u_show_mode, show_mode);

      post_image_uniforms_(p);

      glBindVertexArray(vao_);
      dglDrawArrays(GL_TRIANGLES, 0, 6);
      glBindVertexArray(0);
  }

  /*!
   * Build the occlusion, blur it, and leave it in sao_blur_.
   *
   * copyDepthMip      G-buffer depth into level 0 of the pyramid
   * downSampleDepth   levels 1 to 4, each from the one above
   * sao               the occlusion, sampling a spiral of eleven taps
   * bilateralBlur x2  separable, and depth aware so it does not bleed
   *                   across edges
   */
  bool run_sao_()
  {
      if (!ensure_sao_depth_()
          || !sao_ao_.ensure(width_, height_)
          || !sao_blur_.ensure(width_, height_)) {
          return false;
      }

      // Level 0 is a straight copy of the G-buffer depth into R32F.
      float copy[4] = { static_cast<float>(width_), static_cast<float>(height_),
                        0.0f, 0.0f };

      bind_unit_(0, gdepth_);
      glBindFramebuffer(GL_FRAMEBUFFER, sao_depth_fbo_[0]);
      dglViewport(0, 0, width_, height_);
      sao_copy_.program.use();
      set_mat4(sao_copy_.u_projection, identity_);
      set_mat4(sao_copy_.u_modelview, identity_);
      glUniform2fv(sao_copy_.u_image_size, 1, copy);
      // -1, not 0: that is the value copyDepthMip reads as "straight copy". Any
      // other takes a branch meant for reducing one level into the next, which
      // multiplies a pixel coordinate by the image size and clamps the result
      // into the corner.
      glUniform1i(sao_copy_.u_mip_index, -1);
      glBindVertexArray(vao_);
      dglDrawArrays(GL_TRIANGLES, 0, 6);

      // Then down the pyramid, each level reading the one above it.
      bind_unit_(0, sao_depth_);

      for (int i = 1; i < SAO_MIPS; ++i) {
          int w = width_ >> i;
          int h = height_ >> i;

          if (w < 1) w = 1;
          if (h < 1) h = 1;

          float size[2] = { static_cast<float>(w), static_cast<float>(h) };

          glBindFramebuffer(GL_FRAMEBUFFER, sao_depth_fbo_[i]);
          dglViewport(0, 0, w, h);
          sao_down_.program.use();
          set_mat4(sao_down_.u_projection, identity_);
          set_mat4(sao_down_.u_modelview, identity_);
          set_float(sao_down_.u_view_width, static_cast<float>(w));
          set_float(sao_down_.u_view_height, static_cast<float>(h));
          glUniform2fv(sao_down_.u_image_size, 1, size);
          glUniform1i(sao_down_.u_mip_index, i - 1);
          dglDrawArrays(GL_TRIANGLES, 0, 6);
      }

      // The occlusion itself.
      float sizes[4 * SAO_MIPS] {};

      for (int i = 0; i < SAO_MIPS; ++i) {
          int w = width_ >> i;
          int h = height_ >> i;

          if (w < 1) w = 1;
          if (h < 1) h = 1;

          sizes[i * 4 + 0] = static_cast<float>(w);
          sizes[i * 4 + 1] = static_cast<float>(h);
      }

      // uInvFocalCoords is the tangent of each half field of view, and those
      // are the reciprocals of the projection's two diagonal terms. The engine
      // keeps no far plane, so this is the only way back to an eye position.
      const float* proj = world_projection();
      float focal[2] = {
          (proj[0] != 0.0f) ? (1.0f / proj[0]) : 1.0f,
          (proj[5] != 0.0f) ? (1.0f / proj[5]) : 1.0f
      };

      // uParams1: disk radius, intensity, bias, minimum world radius.
      // uParams2: where the depth bias starts, and how fast a small disk fades.
      // sao.shader negates C.z straight after UVToEyePos, so the max() in the
      // radius expression takes the depth branch and the whole thing collapses
      // to uParams1.x / 2 -- a constant radius in pixels. uParams1.w is only
      // the floor for a surface almost on the camera.
      float radius_px = *r_sao_radius * static_cast<float>(height_);
      // The estimator divides by |Q-C| squared, in world units. At Doom 64 scale
      // that denominator runs into the thousands, so the intensity has to be
      // scaled to match and the bias expressed in world units -- not the small
      // numbers that suit a renderer working in metres.
      float p1[4] = { radius_px * 2.0f, *r_sao_intensity * 12.0f, 2.0f, 0.001f };
      (void)proj;
      float p2[4] = { 0.9f, 3.0f, 0.0f, 0.0f };

      bind_unit_(0, sao_depth_);
      glBindFramebuffer(GL_FRAMEBUFFER, sao_ao_.fbo);
      dglViewport(0, 0, width_, height_);
      sao_.program.use();
      set_mat4(sao_.u_projection, identity_);
      set_mat4(sao_.u_modelview, identity_);
      set_float(sao_.u_view_width, static_cast<float>(width_));
      set_float(sao_.u_view_height, static_cast<float>(height_));
      set_float(sao_.u_zfar, world_far());
      set_vec2(sao_.u_inv_focal, focal);
      set_vec4v(sao_.u_params1, 1, p1);
      set_vec4v(sao_.u_params2, 1, p2);
      set_vec4v(sao_.u_texture_sizes, SAO_MIPS, sizes);
      dglDrawArrays(GL_TRIANGLES, 0, 6);

      // Two blur passes. The weights are a five-tap Gaussian; the shader
      // rejects samples whose depth is too far from the centre, which is what
      // keeps the occlusion from leaking over an edge.
      static const float gauss[6] = {
          0.197448f, 0.174697f, 0.121109f, 0.065591f, 0.027700f, 0.009132f
      };

      float tex_size[2] = { static_cast<float>(width_), static_cast<float>(height_) };

      auto blur = [&](PostProgram& p, Target& dst, GLuint src) {
          bind_unit_(0, src);
          glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
          dglViewport(0, 0, dst.w, dst.h);
          p.program.use();
          set_mat4(p.u_projection, identity_);
          set_mat4(p.u_modelview, identity_);
          glUniform1f(p.u_strides, 1.0f);
          glUniform1fv(p.u_gauss, 6, gauss);
          glUniform2fv(p.u_texture_size, 1, tex_size);
          dglDrawArrays(GL_TRIANGLES, 0, 6);
      };

      // r_GBufferShow 2 alongside r_SAO 2 leaves the occlusion unblurred, so a
      // pass that produces nothing can be told from a blur that erases it.
      if (!(visualize_ao() && *r_gbuffer_show == 2)) {
          blur(blur_h_, sao_blur_, sao_ao_.tex);
          blur(blur_v_, sao_ao_, sao_blur_.tex);
      }

      glBindVertexArray(0);
      return true;
  }

  /*!
   * KEX three SMAA passes, ending with the anti-aliased scene on the window.
   *
   * edgeDetection         luma edges of the scene
   * blendWeights          how much of each neighbour to pull in, from those
   *                       edges and the two reference textures
   * neighborhoodBlending  the blend itself
   *
   * Unlike FXAA, which guesses an edge from one neighbourhood, SMAA looks the
   * pattern up: the area texture holds the precomputed coverage for every
   * shape and distance a line can make against a pixel grid. That is why it
   * keeps detail FXAA smears, and why it cannot work without those textures.
   *
   * src  the finished scene, as a texture
   * @return false if the chain could not run, in which case nothing was drawn
   */
  /*!
   * SMAA's three passes.
   *
   * src  the scene to anti-alias
   * dst  where the result goes, or nullptr for the window. A target when the
   *      menu blur follows, for the same reason SMAA itself needs one: the
   *      next stage samples an image.
   */
  bool run_smaa_(GLuint src, Target* dst)
  {
      if (!ensure_smaa_tex_()
          || !smaa_edges_.ensure(width_, height_)
          || !smaa_blend_.ensure(width_, height_)) {
          return false;
      }

      // vec4(1/w, 1/h, w, h): the first pair offsets texture coordinates, the
      // second recovers a pixel coordinate. Set on each program with that
      // program current -- a uniform belongs to the program, not to the state.
      float rt[4] = {
          1.0f / static_cast<float>(width_),
          1.0f / static_cast<float>(height_),
          static_cast<float>(width_),
          static_cast<float>(height_)
      };

      auto set_rt = [&](PostProgram& p) {
          p.program.use();
          glUniform4fv(p.u_rt_size, 1, rt);
      };

      set_rt(smaa_edge_);
      set_rt(smaa_weights_);
      set_rt(smaa_neighbour_);

      // Both intermediate targets have to start empty. Their shaders discard
      // where they find nothing rather than writing zero, so whatever was left
      // in the texture would be read back as a genuine edge or weight.
      auto clear_target = [&](Target& dst) {
          glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
          dglViewport(0, 0, dst.w, dst.h);
          dglClearColor(0.0f, 0.0f, 0.0f, 0.0f);
          dglClear(GL_COLOR_BUFFER_BIT);
      };

      // 1. Edges.
      bind_unit_(0, src);
      clear_target(smaa_edges_);
      pass_(smaa_edge_, &smaa_edges_, nullptr);

      // 2. Blend weights, against the two reference textures.
      bind_unit_(0, smaa_edges_.tex);
      bind_unit_(1, smaa_area_);
      bind_unit_(2, smaa_search_);
      clear_target(smaa_blend_);
      pass_(smaa_weights_, &smaa_blend_, nullptr);

      // r_SMAA 2 and 3 stop here and show what the two passes produced. Edges
      // should trace the silhouettes and nothing else; weights should glow only
      // along them. A pass that found nothing and one that found everything look
      // identical in the final image, so this is the only way to tell them apart.
      if (*r_smaa_show > 0 && blit_) {
          bind_unit_(1, 0);
          bind_unit_(2, 0);
          bind_unit_(0, (*r_smaa_show > 1) ? smaa_blend_.tex : smaa_edges_.tex);
          pass_(blit_, dst, nullptr, 0.0f);
          bind_unit_(0, 0);
          return true;
      }

      // 3. The blend, on to whatever comes next.
      bind_unit_(0, src);
      bind_unit_(1, smaa_blend_.tex);
      bind_unit_(2, 0);
      pass_(smaa_neighbour_, dst, nullptr);

      bind_unit_(1, 0);
      bind_unit_(0, 0);
      return true;
  }

  /*!
   * KEX's four-pass chain, ending with the blurred scene on the window.
   *
   * pack     full res, velocity encoded to 8 bits with its length and depth
   * tile x2  reduced by TILE on one axis then the other, keeping the longest
   * neighbour  each tile takes the longest of its nine, so motion bleeds out
   * apply    the blur itself, along the tile's velocity
   *
   * dst  where the blurred scene goes, or nullptr for the window. It has
   *      to be a target when SMAA follows, because an anti-aliaser samples
   *      an image and cannot sample a window.
   */
  void run_motion_blur_(Target* dst)
  {
      int tw = (width_ + TILE - 1) / TILE;
      int th = (height_ + TILE - 1) / TILE;

      if (!encode_.ensure(width_, height_)
          || !tile_h_.ensure(tw, height_)
          || !tile_v_.ensure(tw, th)
          || !tile_n_.ensure(tw, th)) {
          return;
      }

      // Pack. The velocity in the G-buffer is a clip-space delta; x turns it
      // into a texture-space one, and y and z cap how far the blur may reach.
      // The cap is the tile size, because that is as far as the field is
      // spread and past it the blur would sample motion it never saw.
      float reach = static_cast<float>(TILE) / static_cast<float>(width_);
      float pack[4] = {
          *r_motionblur_scale * 0.0005f,
          reach,
          reach,
          0.0f
      };

      bind_unit_(0, gvelocity_);
      bind_unit_(1, gdepth_);
      pass_(mb_pack_, &encode_, pack);

      // Tiles. The shader walks TILE texels of the source with texelFetch, so
      // it is given the source size in texels and which way to walk.
      float tile_x[4] = { static_cast<float>(width_), static_cast<float>(height_),
                          static_cast<float>(TILE), 0.0f };
      bind_unit_(0, encode_.tex);
      pass_(mb_tile_, &tile_h_, tile_x);

      float tile_y[4] = { static_cast<float>(tw), static_cast<float>(height_),
                          static_cast<float>(TILE), 1.0f };
      bind_unit_(0, tile_h_.tex);
      pass_(mb_tile_, &tile_v_, tile_y);

      // Neighbourhood: one texel step in the tile image.
      float step[4] = { 1.0f / static_cast<float>(tw), 1.0f / static_cast<float>(th),
                        0.0f, 0.0f };
      bind_unit_(0, tile_v_.tex);
      pass_(mb_neighbour_, &tile_n_, step);

      // Apply, straight to the window. Tile size in texture coordinates.
      float apply[4] = { static_cast<float>(TILE) / static_cast<float>(width_),
                         static_cast<float>(TILE) / static_cast<float>(height_),
                         0.0f, 0.0f };

      bind_unit_(0, color_);
      bind_unit_(1, encode_.tex);
      bind_unit_(2, tile_n_.tex);
      pass_(mb_apply_, dst, apply);

      // r_MotionBlur 2 draws the tile grid and each tile's velocity over the
      // top. It is the only way to see what the chain decided.
      if (visualize_motion_blur() && mb_show_) {
          dglEnable(GL_BLEND);
          dglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
          bind_unit_(0, tile_n_.tex);
          pass_(mb_show_, dst, apply);
          dglDisable(GL_BLEND);
      }

      bind_unit_(2, 0);
      bind_unit_(1, 0);
      bind_unit_(0, 0);

      glUseProgram(0);
  }

  /*!
   * Which program brings the buffer back to the window.
   *
   * A debug view wins over an anti-aliaser: there is no point smoothing a
   * velocity field. With neither, and the G-buffer on, it is still a straight
   * copy -- the scene has to get out of the framebuffer somehow.
   */
  bool dyn_scale_on();

  PostProgram* selected_()
  {
      if (*r_gbuffer && *r_gbuffer_show)
          return blit_ ? &blit_ : nullptr;

      switch (*r_antialiasing) {
      case 1:  if (fxaa_) return &fxaa_; break;
      case 2:  if (fxaa_fast_) return &fxaa_fast_; break;
      default: break;
      }

      // The dynamic controller keeps the chain engaged even on a frame with
      // nothing to do, so that there is something to time. Something still has
      // to put the scene back on the window afterwards, or it stays in the
      // off-screen target and the player gets a black world with a HUD on it.
      if (dyn_scale_on())
          return blit_ ? &blit_ : nullptr;

      return nullptr;
  }

  /*!
   * progs/simpleBlur.shader, over the finished image.
   *
   * uParams is (width, height, radius, cutoff squared). The shader walks a
   * square of taps two pixels apart out to the radius and weights each by
   * sqrt(uParams.w) minus its distance from the centre, so the fourth component
   * decides where that weight reaches zero.
   *
   * The furthest taps are the four corners, at radius * sqrt(2). Putting the
   * cutoff exactly there would give them a weight of zero -- and at radius 1,
   * where the corners are the only taps, every weight would be zero and the
   * result black. So the cutoff goes one ring further out, at
   * (radius + 1) * sqrt(2): every tap then contributes, the corners least.
   *
   * That is a choice, not something read off KEX. Their shader is used
   * unmodified; what to feed it is ours to decide.
   */
  void run_menu_blur_(GLuint src, int radius, Target* dst)
  {
      float cutoff = static_cast<float>(radius + 1) * 1.41421356f;

      float params[4] = {
          static_cast<float>(width_),
          static_cast<float>(height_),
          static_cast<float>(radius),
          cutoff * cutoff
      };

      // Posted with the program current, like SMAA's uRTSize: a uniform belongs
      // to the program, and pass_ is what makes this one current.
      blur_simple_.program.use();
      glUniform4fv(blur_simple_.u_blur_params, 1, params);

      bind_unit_(0, src);
      pass_(blur_simple_, dst, nullptr);
      bind_unit_(0, 0);
  }

  /*!
   * The stretch: the finished scene, at its reduced size, on to the window.
   *
   * One extra full-screen pass, and the only one that ever crosses resolutions.
   * gbuffer.shader samples through the quad's own texture coordinates rather
   * than from the fragment position, so mapping a small texture onto a large
   * viewport is simply what it does, and GL_LINEAR on the target does the
   * filtering.
   */
  void upscale_to_window_(GLuint src)
  {
      if (!blit_)
          return;

      bind_unit_(0, src);
      pass_(blit_, nullptr, nullptr, 0.0f);
      bind_unit_(0, 0);
  }

  /*!
   * Run whatever chain is enabled over the scene.
   *
   * The order is deliberate. Motion blur runs first: it belongs to the scene, a
   * consequence of things having moved. Anti-aliasing runs after, because it is
   * a property of the image actually being shown -- smoothing edges the blur is
   * about to smear would be work thrown away. The menu blur is last of all, and
   * is not about the scene at all.
   *
   * Every stage but the last writes into a target, because the stage after it
   * samples an image and a window cannot be sampled.
   *
   * @param to_target Keep the result in a target instead of putting it on the
   *                  window, because something still has to happen to it: a
   *                  menu blur, a stretch to the window, or both.
   *
   * @return where the scene ended up: nullptr if it is already on the window,
   *         or the target still owing that last stage. The caller composes the
   *         ambient occlusion into it first, so that the occlusion is blurred
   *         and stretched along with everything else instead of sitting sharp
   *         on top of a soft image.
   */
  Target* present_scene_(bool blur, bool smaa, bool to_target)
  {
      if (to_target && !menu_blur_src_.ensure(width_, height_))
          to_target = false;

      Target* tail = to_target ? &menu_blur_src_ : nullptr;

      GLuint src = color_;

      if (blur) {
          // Motion blur goes to the tail unless SMAA is between the two.
          Target* dst = tail;

          if (smaa) {
              if (scene_.ensure(width_, height_))
                  dst = &scene_;
              else
                  smaa = false;   // nothing to hand it, so it cannot run
          }

          run_motion_blur_(dst);

          if (!dst)
              return nullptr;     // straight to the window

          src = dst->tex;

          if (dst == tail)
              return tail;
      }

      if (smaa && run_smaa_(src, tail))
          return tail;

      // Nothing claimed the frame, so the scene still has to get out of the
      // framebuffer somehow.
      PostProgram* p = selected_();

      if (!p)
          p = blit_ ? &blit_ : nullptr;

      if (p) {
          bind_unit_(0, src);
          pass_(*p, tail, nullptr, 0.0f);
          return tail;
      }

      // No program at all: the scene never left the framebuffer, so there is
      // nothing in the tail for a blur to read.
      return nullptr;
  }

  void destroy_targets_()

  {
      if (color_) {
          dglDeleteTextures(1, &color_);
          color_ = 0;
      }

      if (gdepth_) {
          dglDeleteTextures(1, &gdepth_);
          gdepth_ = 0;
      }

      if (gvelocity_) {
          dglDeleteTextures(1, &gvelocity_);
          gvelocity_ = 0;
      }

      if (depth_) {
          glDeleteRenderbuffers(1, &depth_);
          depth_ = 0;
      }

      if (fbo_) {
          glDeleteFramebuffers(1, &fbo_);
          fbo_ = 0;
      }

      encode_.destroy();
      tile_h_.destroy();
      tile_v_.destroy();
      tile_n_.destroy();
      sao_ao_.destroy();
      sao_blur_.destroy();
      destroy_sao_depth_();

      // The two SMAA reference textures are deliberately not freed here:
      // they do not depend on the window size, and reloading them on every
      // resize would be work for nothing.
      smaa_edges_.destroy();
      smaa_blend_.destroy();
      scene_.destroy();
      menu_blur_src_.destroy();
      menu_blur_dst_.destroy();

      width_ = height_ = 0;
      out_width_ = out_height_ = 0;
      targets_mrt_ = false;
  }

  /*! One half-float colour target, set up the way the post shaders read it. */
  GLuint make_target_(int w, int h)
  {
      GLuint id = 0;

      dglGenTextures(1, &id);
      dglBindTexture(GL_TEXTURE_2D, id);
      dglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0,
                    GL_RGBA, GL_HALF_FLOAT, nullptr);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, DGL_CLAMP);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, DGL_CLAMP);
      dglBindTexture(GL_TEXTURE_2D, 0);

      return id;
  }

  /*! The scale the controller has settled on, 0.25 to 1. See run_dyn_scale_. */
  float dyn_scale_ { 1.0f };

  void run_dyn_scale_();

  /*! The clamped scale the scene should be rendered at. */
  float render_scale_()
  {
      float s = *r_resolutionscale_dynamic ? dyn_scale_ : *r_resolutionscale;

      if (s > 1.0f) return 1.0f;
      if (s < 0.25f) return 0.25f;

      return s;
  }

  /*!
   * How long the GPU spent on the last frame it finished, as a fraction of the
   * budget. Zero until a query has come back.
   *
   * The measurement has to be the GPU's, not the CPU's. Wall time around the
   * draw calls measures how fast the driver accepted commands, which on a
   * GPU-bound frame -- the only kind worth scaling for -- is a small fraction of
   * the real cost. A controller fed that number would keep the resolution
   * pinned at 1 while the frame rate collapsed.
   */
  float time_fraction_ {};

  /*! Consecutive frames over the target, and under the good mark. */
  int over_frames_ {};
  int under_frames_ {};

  /* Three queries in rotation. One is being written, the others are ripening;
   * by the time the ring comes round the oldest has long been available, so
   * the result is never waited on. A stall here would be the controller
   * causing the very thing it exists to prevent. */
  enum { TIMER_QUERIES = 3 };
  GLuint timer_query_[TIMER_QUERIES] {};
  bool timer_active_[TIMER_QUERIES] {};
  int timer_head_ {};
  bool timer_open_ {};
  bool timer_ready_ {};

  /*! Nanoseconds one frame is allowed, from the display's own refresh rate. */
  double frame_budget_ns_()
  {
      static double cached = 0.0;

      if (cached == 0.0) {
          SDL_DisplayMode mode {};
          int hz = 0;

          if (SDL_GetCurrentDisplayMode(0, &mode) == 0)
              hz = mode.refresh_rate;

          // 0 means SDL does not know, which is legal and happens on some
          // drivers. 60 is the assumption everything else in the engine makes.
          if (hz <= 0)
              hz = 60;

          cached = 1000000000.0 / static_cast<double>(hz);
      }

      return cached;
  }

  /*! Whether the dynamic controller can run at all. */
  bool dyn_scale_on()
  {
      return *r_resolutionscale_dynamic && timer_ready_;
  }

  /*!
   * Start timing the frame. Paired with timer_end_.
   */
  void timer_begin_()
  {
      if (!dyn_scale_on() || timer_open_)
          return;

      glBeginQuery(GL_TIME_ELAPSED, timer_query_[timer_head_]);
      timer_open_ = true;
  }

  /*!
   * Stop timing, then collect whichever earlier frame has finished.
   */
  void timer_end_()
  {
      if (!timer_open_)
          return;

      glEndQuery(GL_TIME_ELAPSED);
      timer_open_ = false;
      timer_active_[timer_head_] = true;

      timer_head_ = (timer_head_ + 1) % TIMER_QUERIES;

      // The slot about to be reused is the oldest. Take its answer if it has
      // one; never block for it.
      if (timer_active_[timer_head_]) {
          GLint done = 0;

          glGetQueryObjectiv(timer_query_[timer_head_], GL_QUERY_RESULT_AVAILABLE, &done);

          if (done) {
              GLuint64 ns = 0;

              glGetQueryObjectui64v(timer_query_[timer_head_], GL_QUERY_RESULT, &ns);
              timer_active_[timer_head_] = false;

              time_fraction_ =
                  static_cast<float>(static_cast<double>(ns) / frame_budget_ns_());

              run_dyn_scale_();
          }
      }
  }

  /*!
   * One step of the controller, run once per timed frame.
   *
   * Between the good mark and the target is the dead band, and a frame landing
   * there resets both counters. Without it the two counters would race -- a
   * frame that is neither good nor bad would still be evidence for one of
   * them, and the scale would drift on noise alone.
   */
  void run_dyn_scale_()
  {
      float was = dyn_scale_;
      float target = *r_resolutionscale_targetdrawtime;
      float good = *r_resolutionscale_gooddrawtime;

      if (target < 0.1f) target = 0.1f;
      if (good > target) good = target;

      int wait_down = *r_resolutionscale_numframesbeforelowering;
      int wait_up = *r_resolutionscale_numframesbeforeraising;
      float step_down = *r_resolutionscale_lowerspeed;
      float step_up = *r_resolutionscale_increasespeed;

      // Aggressive halves both waits and lets the drop scale with how far past
      // the target the frame landed, so a frame at twice the budget loses twice
      // as much resolution instead of grinding down 0.1 at a time.
      if (*r_resolutionscale_aggressive) {
          wait_down /= 2;
          wait_up /= 2;
          step_down *= (time_fraction_ / target);
      }

      if (wait_down < 1) wait_down = 1;
      if (wait_up < 1) wait_up = 1;

      if (time_fraction_ > target) {
          under_frames_ = 0;

          if (++over_frames_ >= wait_down) {
              over_frames_ = 0;
              dyn_scale_ -= step_down;
          }
      }
      else if (time_fraction_ < good) {
          over_frames_ = 0;

          if (++under_frames_ >= wait_up) {
              under_frames_ = 0;
              dyn_scale_ += step_up;
          }
      }
      else {
          over_frames_ = 0;
          under_frames_ = 0;
      }

      if (dyn_scale_ > 1.0f) dyn_scale_ = 1.0f;
      if (dyn_scale_ < 0.25f) dyn_scale_ = 0.25f;

      // Rare by construction -- the waits are 20 and 200 frames -- so this is a
      // record of what the controller decided rather than a stream.
      if (dyn_scale_ != was) {
          log::info("Resolution scale {:.3f} -> {:.3f} (time fraction {:.3f})",
                    was, dyn_scale_, time_fraction_);
      }
  }

  /*!
   * The scene size for a given window size.
   *
   * Rounded to even numbers: the motion blur reduces by twenty on each axis and
   * SAO builds a five-level pyramid by halving, and an odd size makes those
   * divisions disagree by a pixel between passes.
   */
  void scaled_size_(int win_w, int win_h, int& w, int& h)
  {
      float s = render_scale_();

      w = static_cast<int>(win_w * s) & ~1;
      h = static_cast<int>(win_h * s) & ~1;

      if (w < 2) w = 2;
      if (h < 2) h = 2;
  }

  /*! Whether the scene is being rendered smaller than the window. */
  bool scaling_active()
  {
      return width_ > 0 && (width_ != out_width_ || height_ != out_height_);
  }

  /*!
   * Make the off-screen target match the window, or the fraction of it that
   * r_ResolutionScale asks for.
   *
   * @return true if it is usable
   */
  bool ensure_targets_()
  {
      // The blur reads the velocity and depth targets, so asking for it asks
      // for the G-buffer. Nothing makes the player set two cvars for one
      // effect.
      bool want_mrt = (*r_gbuffer != 0) || (*r_motionblur != 0) || (*r_sao != 0)
                      || *r_motionblurvisualize || *r_visualizeao;

      if (video_width <= 0 || video_height <= 0)
          return false;

      int want_w, want_h;
      scaled_size_(video_width, video_height, want_w, want_h);

      if (fbo_ && width_ == want_w && height_ == want_h
          && out_width_ == video_width && out_height_ == video_height
          && targets_mrt_ == want_mrt) {
          return true;
      }

      destroy_targets_();

      out_width_ = video_width;
      out_height_ = video_height;
      width_ = want_w;
      height_ = want_h;

      dglGenTextures(1, &color_);
      dglBindTexture(GL_TEXTURE_2D, color_);
      dglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

      // The post shaders sample neighbours by hand, so no filtering beyond
      // linear, and no wrapping past the edge.
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, DGL_CLAMP);
      dglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, DGL_CLAMP);
      dglBindTexture(GL_TEXTURE_2D, 0);

      glGenRenderbuffers(1, &depth_);
      glBindRenderbuffer(GL_RENDERBUFFER, depth_);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                  width_, height_);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);

      glGenFramebuffers(1, &fbo_);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, color_, 0);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, depth_);

      targets_mrt_ = want_mrt;

      if (want_mrt) {
          gdepth_ = make_target_(width_, height_);
          gvelocity_ = make_target_(width_, height_);

          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                       GL_TEXTURE_2D, gdepth_, 0);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                                       GL_TEXTURE_2D, gvelocity_, 0);
      }

      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      if (status != GL_FRAMEBUFFER_COMPLETE) {
          log::warn("Post-process framebuffer incomplete (0x{:x}); disabling",
                    static_cast<unsigned>(status));
          destroy_targets_();
          return false;
      }

      log::info("Post-process target: {}x{}{}{}", width_, height_,
                scaling_active()
                    ? fmt::format(" (scaled from {}x{})", out_width_, out_height_)
                    : String{},
                want_mrt ? " (G-buffer, 3 attachments)" : "");
      return true;
  }

  void ensure_quad_()
  {
      if (vao_)
          return;

      // Two triangles covering clip space, with the matching texture
      // coordinates. x, y, z, u, v.
      static const float quad[] = {
          -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
           1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
           1.0f,  1.0f, 0.0f,  1.0f, 1.0f,

          -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
           1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
          -1.0f,  1.0f, 0.0f,  0.0f, 1.0f
      };

      glGenVertexArrays(1, &vao_);
      glGenBuffers(1, &vbo_);

      glBindVertexArray(vao_);
      glBindBuffer(GL_ARRAY_BUFFER, vbo_);
      glBufferData(GL_ARRAY_BUFFER, static_cast<gl33::sizeiptr>(sizeof(quad)),
                         quad, GL_STATIC_DRAW);

      // Matches common.inc: 0 position, 1 texcoord.
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                                  reinterpret_cast<void*>(0));

      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                                  reinterpret_cast<void*>(3 * sizeof(float)));

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

//
// CMD_StatResolutionScale
//
// KEX prints this under "stat resolutionscale", with these four columns. It is
// the only way to see what the controller is doing: the scale it settles on is
// a number nothing else on screen reports, and judging it by how blocky the
// picture looks is not judging it at all.
//

static CMD(StatResolutionScale)
{
    (void)param;

    if (!*r_resolutionscale_dynamic) {
        I_Printf("Resolution Scale: dynamic scaling is off (r_ResolutionScaleDynamic)\n");
    }
    else if (!timer_ready_) {
        I_Printf("Resolution Scale: unavailable, no GPU timer queries\n");
    }

    I_Printf("Resolution Scale\n");
    I_Printf("  Active Scale      %.3f\n", render_scale_());
    I_Printf("  Time Fraction     %.3f  (target %.3f, good %.3f)\n",
             time_fraction_,
             (float)*r_resolutionscale_targetdrawtime,
             (float)*r_resolutionscale_gooddrawtime);
    I_Printf("  Width Percentage  %.1f%%\n",
             out_width_ ? 100.0f * (float)width_ / (float)out_width_ : 100.0f);
    I_Printf("  Height Percentage %.1f%%\n",
             out_height_ ? 100.0f * (float)height_ / (float)out_height_ : 100.0f);
    I_Printf("  Frames over %i, under %i\n", over_frames_, under_frames_);
}

void shader::init_post()
{
    G_AddCommand("statresolutionscale", CMD_StatResolutionScale, 0);

    cvar::Register()
        (r_antialiasing, "r_AntiAliasing",
         "Sets an antialiasing mode: 0 off, 1 FXAA, 2 fast FXAA, 3 SMAA")
        (r_gbuffer, "r_GBuffer", "Write the colour/depth/velocity G-buffer")
        (r_motionblur, "r_MotionBlur",
         "Per-object motion blur: 0 off, 1 on, 2 on with the tile overlay")
        (r_motionblur_scale, "r_MotionBlurScale", "How far the blur reaches")
        (r_sao, "r_SAO", "Ambient occlusion: 0 off, 1 on, 2 show the occlusion")
        (r_sao_radius, "r_MaxOcclusionUnit", "World radius the occlusion samples")
        (r_sao_intensity, "r_SAOIntensity", "How dark the occlusion goes")
        (r_menublur, "r_MenuBlur",
         "Radius in pixels of the blur behind the menu, 0 off")
        (r_resolutionscale, "r_ResolutionScale",
         "Fraction of the window the scene is rendered at, 1 is native")
        (r_resolutionscale_dynamic, "r_ResolutionScaleDynamic",
         "Adjust the resolution scale automatically from how long the GPU takes")
        (r_resolutionscale_targetdrawtime, "r_ResolutionScaleTargetDrawTime",
         "Draw time, as a fraction of the frame budget, above which the resolution is lowered")
        (r_resolutionscale_gooddrawtime, "r_ResolutionScaleGoodDrawTime",
         "Draw time the frame must fall under, as a fraction of the frame budget, before the resolution is raised")
        (r_resolutionscale_increasespeed, "r_ResolutionScaleIncreaseSpeed",
         "How much scale to add each time the resolution is raised")
        (r_resolutionscale_lowerspeed, "r_ResolutionScaleLowerSpeed",
         "How much scale to remove each time the resolution is lowered")
        (r_resolutionscale_aggressive, "r_ResolutionScaleAggressive",
         "Halve both waits and let the drop grow with how far over budget the frame ran")
        (r_resolutionscale_numframesbeforelowering, "r_ResolutionScaleNumFramesBeforeLowering",
         "Number of frames to wait before decreasing resolution when performance is bad")
        (r_resolutionscale_numframesbeforeraising, "r_ResolutionScaleNumFramesBeforeRaising",
         "Number of frames to wait before increasing resolution when performance is good")
        (r_brightness, "r_Brightness",
         "Environmental brightness: multiplies the world image, 1 is neutral");

    // Not saved. A debug view that survives a restart is a trap: the engine
    // comes back up drawing a depth buffer, which looks exactly like a
    // rendering bug and sends whoever sees it hunting for one. It is a
    // developer tool, so it lasts as long as the session that asked for it.
    cvar::Register{ cvar::Flag::noconfig }
        (r_gbuffer_show, "r_GBufferShow",
         "Show a G-buffer target: 0 normal, 1 depth, 2 velocity, 3 mask")
        (r_smaa_show, "r_SMAAShow",
         "Show what SMAA computed: 0 normal, 1 the edges, 2 the blend weights")
        (r_visualizeao, "r_VisualizeAO",
         "Show the ambient occlusion instead of applying it")
        (r_motionblurvisualize, "r_MotionBlurVisualize",
         "Draw the velocity tile grid and each tile's motion over the scene")
        (r_showvirtualvram, "r_ShowVirtualVRAM",
         "Display a layer of the virtual VRAM texture: 0 off, 1 the first layer");

    if (!gl33::loaded() || !glGenFramebuffers)
        return;

    try {
        Preprocessor pp;
        configure_preprocessor(pp);

        fxaa_.program = Program::build("progs/fxaa.shader", pp);
        fxaa_fast_.program = Program::build("progs/fxaa_fast.shader", pp);
        blit_.program = Program::build("progs/d64ex/gbuffer.shader", pp);

        // motionBlurMain, through five of its entry points. Each is a
        // #define plus an #include, so this is one shader compiled five ways.
        mb_pack_.program = Program::build("progs/velocityPack.shader", pp);
        mb_tile_.program = Program::build("progs/velocityTileGen.shader", pp);
        mb_neighbour_.program = Program::build("progs/velocityTileNeighborhood.shader", pp);
        mb_apply_.program = Program::build("progs/motionBlur.shader", pp);
        mb_show_.program = Program::build("progs/motionBlurVisualize.shader", pp);

        sao_copy_.program = Program::build("progs/copyDepthMip.shader", pp);
        sao_down_.program = Program::build("progs/downSampleDepth.shader", pp);
        sao_.program = Program::build("progs/sao.shader", pp);
        blur_h_.program = Program::build("progs/bilateralBlur_H.shader", pp);
        blur_v_.program = Program::build("progs/bilateralBlur_V.shader", pp);

        blur_simple_.program = Program::build("progs/simpleBlur.shader", pp);
        vram_.program = Program::build("progs/doomDisplayTexArray.shader", pp);
    } catch (const std::exception& e) {
        log::warn("Post-process unavailable: {}", e.what());
        return;
    }

    // SMAA on its own, because it is the one effect that can legitimately be
    // missing: its two reference textures are not in the kpf. A failure here
    // must cost SMAA and nothing else, so it does not share the try above.
    try {
        Preprocessor pp;
        configure_preprocessor(pp);

        smaa_edge_.program =
            Program::build("progs/SMAA/SMAA_edgeDetection.shader", pp);
        smaa_weights_.program =
            Program::build("progs/SMAA/SMAA_blendWeights.shader", pp);
        smaa_neighbour_.program =
            Program::build("progs/SMAA/SMAA_neighborhoodBlending.shader", pp);

        locate_(smaa_edge_);
        locate_(smaa_weights_);
        locate_(smaa_neighbour_);
    } catch (const std::exception& e) {
        log::warn("SMAA unavailable: {}", e.what());
    }

    locate_(fxaa_);
    locate_(fxaa_fast_);
    locate_(blit_);
    locate_(mb_pack_);
    locate_(mb_tile_);
    locate_(mb_neighbour_);
    locate_(mb_apply_);
    locate_(mb_show_);
    locate_(sao_copy_);
    locate_(sao_down_);
    locate_(sao_);
    locate_(blur_h_);
    locate_(blur_v_);
    locate_(blur_simple_);
    locate_(vram_);

    // GL_TIME_ELAPSED is core since 3.3, but a driver that resolved the rest
    // and not these would take the controller down with it, so it is asked for
    // rather than assumed. Without it r_ResolutionScaleDynamic simply does
    // nothing, and says so once.
    if (glGenQueries && glBeginQuery && glEndQuery
        && glGetQueryObjectiv && glGetQueryObjectui64v) {
        glGenQueries(TIMER_QUERIES, timer_query_);
        timer_ready_ = timer_query_[0] != 0;
    }

    if (!timer_ready_)
        log::warn("No GPU timer queries: r_ResolutionScaleDynamic unavailable");

    ready_ = true;

    log::info("Post-process ready: {} + {}",
              fxaa_.program.path(), fxaa_fast_.program.path());
}

bool shader::post_active()
{
    if (!ready_ || !draw_ready())
        return false;

    return selected_() != nullptr || *r_motionblur != 0 || *r_sao != 0
           || *r_motionblurvisualize || *r_visualizeao || *r_antialiasing != 0
           || menu_blur_on() || render_scale_() < 1.0f || dyn_scale_on()
           || env_brightness_on() || gamma_on();
}

bool shader::post_begin()
{
    bound_ = false;

    if (!post_active())
        return false;

    if (!ensure_targets_()) {
        if (!warned_) {
            warned_ = true;
            log::warn("Post-process disabled: no usable framebuffer");
        }
        return false;
    }

    ensure_quad_();
    timer_begin_();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // A framebuffer draws only to the buffers it is told to. Without this the
    // world shader's outFragment1 and outFragment2 would go nowhere.
    active_mrt_ = targets_mrt_;

    if (active_mrt_) {
        static const GLenum bufs[3] = {
            GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
        };
        glDrawBuffers(3, bufs);
    }

    // Borrow the engine's viewport globals for the length of the world pass.
    //
    // GL_ClearView, GL_ResetViewport, the sky and the sprite path all take
    // their viewport from these four and from nothing else, so pointing them at
    // the target is what makes the scene render at the reduced size -- without
    // a single call site having to know that resolution scaling exists. They go
    // back in post_end, before anything is drawn to the window.
    saved_view_[0] = ViewWidth;
    saved_view_[1] = ViewHeight;
    saved_view_[2] = ViewWindowX;
    saved_view_[3] = ViewWindowY;
    saved_view_[4] = video_width;
    saved_view_[5] = video_height;
    view_borrowed_ = true;

    ViewWidth = width_;
    ViewHeight = height_;
    ViewWindowX = 0;
    ViewWindowY = 0;
    video_width = width_;
    video_height = height_;

    // The engine only clears the view rectangle, which is smaller than the
    // window whenever the player has shrunk the screen. Everything outside it
    // would then be last frame's pixels, and the full-screen quad would hand
    // them straight back. Clear the whole attachment first.
    {
        GLboolean scissor_was = dglIsEnabled(GL_SCISSOR_TEST);

        dglDisable(GL_SCISSOR_TEST);
        dglViewport(0, 0, width_, height_);
        dglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        dglClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (scissor_was)
            dglEnable(GL_SCISSOR_TEST);
    }

    bound_ = true;
    return true;
}

static void post_end_body_()
{
    if (!bound_)
        return;

    bound_ = false;
    active_mrt_ = false;

    // Hand the viewport globals back before anything targets the window.
    if (view_borrowed_) {
        ViewWidth    = saved_view_[0];
        ViewHeight   = saved_view_[1];
        ViewWindowX  = saved_view_[2];
        ViewWindowY  = saved_view_[3];
        video_width  = saved_view_[4];
        video_height = saved_view_[5];
        view_borrowed_ = false;

        // And the scissor box with them. GL_ClearView sets it from those four
        // globals, so while they were borrowed it was clipped to the reduced
        // target -- and the status bar, drawn later and to the window, would
        // have been cut off at the scene's width. It cost an ARMOR counter to
        // find out.
        dglScissor(ViewWindowX, ViewWindowY, ViewWidth, ViewHeight);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    bool sao = sao_on();
    bool blur = motion_blur_on();
    bool smaa = smaa_on();
    bool menu_blur = menu_blur_on();
    bool upscale = scaling_active();

    // The stretch has to go through this block too: it needs the chain kept in a
    // target, and the simple path below writes straight to the window.
    if (sao || blur || smaa || menu_blur || upscale || env_brightness_on()
        || gamma_on()) {
        GLboolean depth_was = dglIsEnabled(GL_DEPTH_TEST);
        GLboolean blend_was = dglIsEnabled(GL_BLEND);
        GLboolean scissor_was = dglIsEnabled(GL_SCISSOR_TEST);
        GLboolean cull_was = dglIsEnabled(GL_CULL_FACE);

        dglDisable(GL_DEPTH_TEST);
        dglDisable(GL_BLEND);
        dglDisable(GL_SCISSOR_TEST);
        dglDisable(GL_CULL_FACE);

        // Occlusion first: it is computed from the G-buffer, before anything
        // touches the colour.
        bool have_sao = sao && run_sao_();

        // r_SAO 2 puts the occlusion on the screen instead of the scene. It is
        // the only way to judge the radius and the intensity, which are the two
        // settings that decide whether this looks like shadow or like grime.
        if (have_sao && visualize_ao() && blit_) {
            bool pyramid = (*r_gbuffer_show == 1);

            // Mode 4 splats red across the channels. The pyramid is R32F and
            // reads the same way, so one mode serves both.
            bind_unit_(0, pyramid ? sao_depth_ : sao_ao_.tex);
            pass_(blit_, nullptr, nullptr, 4.0f);
        }
        else {
            // Where the chain left the scene: the window, or a target that
            // still owes a menu blur, a stretch, or both.
            Target* pending = present_scene_(blur, smaa, menu_blur || upscale);

            // Then multiply the occlusion over it. A blend rather than another
            // shader: the occlusion is already a greyscale image, and
            // GL_ZERO/GL_SRC_COLOR is exactly what darkening means.
            if (have_sao && blit_) {
                dglEnable(GL_BLEND);
                dglBlendFunc(GL_ZERO, GL_SRC_COLOR);

                // Mode 4 again: multiplying by vec4(occlusion, depth, 0, 0)
                // would leave green tinted by depth and kill blue outright.
                bind_unit_(0, sao_ao_.tex);
                pass_(blit_, pending, nullptr, 4.0f);

                dglDisable(GL_BLEND);
                dglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }

            // Then the menu blur, now that the occlusion is part of the image,
            // and last of all the stretch to the window. The blur runs at the
            // scene's size rather than the window's -- it is a blur, so there
            // is nothing to gain from doing it on more pixels.
            if (pending && menu_blur) {
                Target* dst = nullptr;

                if (upscale && menu_blur_dst_.ensure(width_, height_))
                    dst = &menu_blur_dst_;

                run_menu_blur_(pending->tex, scene_blur_radius(), dst);
                pending = dst;
            }

            if (pending && upscale)
                upscale_to_window_(pending->tex);
        }

        bind_unit_(0, 0);
        glUseProgram(0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        dglViewport(0, 0, out_width_, out_height_);

        if (depth_was)   dglEnable(GL_DEPTH_TEST);
        if (blend_was)   dglEnable(GL_BLEND);
        if (scissor_was) dglEnable(GL_SCISSOR_TEST);
        if (cull_was)    dglEnable(GL_CULL_FACE);
        return;
    }

    PostProgram* p = selected_();
    if (!p)
        return;

    // The quad covers the window and replaces it outright: no depth, no
    // blending, no scissor, no culling. Whatever the world left enabled would
    // otherwise decide whether it draws at all.
    GLboolean depth_was = dglIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_was = dglIsEnabled(GL_BLEND);
    GLboolean scissor_was = dglIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull_was = dglIsEnabled(GL_CULL_FACE);

    dglDisable(GL_DEPTH_TEST);
    dglDisable(GL_BLEND);
    dglDisable(GL_SCISSOR_TEST);
    dglDisable(GL_CULL_FACE);

    dglViewport(0, 0, out_width_, out_height_);

    p->program.use();

    set_mat4(p->u_projection, identity_);
    set_mat4(p->u_modelview, identity_);
    set_float(p->u_view_width, static_cast<float>(width_));
    set_float(p->u_view_height, static_cast<float>(height_));

    if (p->u_show_mode >= 0) {
        glUniform1f(p->u_show_mode,
                          targets_mrt_ ? static_cast<float>(*r_gbuffer_show) : 0.0f);
    }

    post_image_uniforms_(*p);

    if (targets_mrt_) {
        glActiveTexture(GL_TEXTURE0 + 2);
        dglBindTexture(GL_TEXTURE_2D, gvelocity_);
        glActiveTexture(GL_TEXTURE0 + 1);
        dglBindTexture(GL_TEXTURE_2D, gdepth_);
    }

    glActiveTexture(GL_TEXTURE0);
    dglBindTexture(GL_TEXTURE_2D, color_);

    glBindVertexArray(vao_);
    dglDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
    dglBindTexture(GL_TEXTURE_2D, 0);

    if (targets_mrt_) {
        glActiveTexture(GL_TEXTURE0 + 1);
        dglBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0 + 2);
        dglBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }

    if (depth_was)   dglEnable(GL_DEPTH_TEST);
    if (blend_was)   dglEnable(GL_BLEND);
    if (scissor_was) dglEnable(GL_SCISSOR_TEST);
    if (cull_was)    dglEnable(GL_CULL_FACE);
}

//
// The body is separate only so the timer can be closed on every path out of it.
// post_end has three returns, and a GL_TIME_ELAPSED query left open would take
// the next glBeginQuery down with it -- GL allows exactly one query of a target
// at a time.
//
void shader::post_end()
{
    post_end_body_();
    timer_end_();
}

void shader::post_show_vram()
{
    if (!ready_ || !vram_ || !atlas_ready())
        return;

    int layers = static_cast<int>(atlas_layers());
    int want = *r_showvirtualvram;

    if (want < 1 || layers < 1)
        return;

    if (want > layers)
        want = layers;

    // The quad belongs to post_begin, which only runs when an effect asked for
    // render targets. This view has to work with every effect switched off --
    // which is exactly how anyone will use it -- so it makes its own.
    ensure_quad_();

    if (!vao_)
        return;

    GLboolean depth_was = dglIsEnabled(GL_DEPTH_TEST);
    GLboolean blend_was = dglIsEnabled(GL_BLEND);
    GLboolean scissor_was = dglIsEnabled(GL_SCISSOR_TEST);
    GLboolean cull_was = dglIsEnabled(GL_CULL_FACE);

    // Everything the interface is about to be drawn with is left exactly as it
    // was found. A debug view that quietly moves the viewport takes the status
    // bar with it, and then it is the view that looks like the bug.
    GLint viewport_was[4] = { 0, 0, 0, 0 };
    dglGetIntegerv(GL_VIEWPORT, viewport_was);

    dglDisable(GL_DEPTH_TEST);
    dglDisable(GL_BLEND);
    dglDisable(GL_SCISSOR_TEST);
    dglDisable(GL_CULL_FACE);

    // video_width, not out_width_: that one is only assigned alongside the
    // render targets, and there may not be any.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    dglViewport(0, 0, video_width, video_height);

    vram_.program.use();
    set_mat4(vram_.u_projection, identity_);
    set_mat4(vram_.u_modelview, identity_);

    // The array and a plain 2D texture would both be bound on unit 0 otherwise,
    // and a unit with two targets bound is undefined the moment the shader
    // samples one of them.
    glActiveTexture(GL_TEXTURE0);
    dglBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, atlas_texture());

    // Their shader reads the layer from the vertex colour: fLayer = colour.r *
    // 255. This VAO enables position and texcoord only, so attribute 2 falls
    // back to its current generic value and every vertex sees the same layer --
    // which is exactly what is wanted, and costs no vertex buffer of its own.
    glVertexAttrib4f(2, static_cast<float>(want - 1) / 255.0f, 0.0f, 0.0f, 1.0f);

    glBindVertexArray(vao_);
    dglDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glUseProgram(0);

    dglViewport(viewport_was[0], viewport_was[1], viewport_was[2], viewport_was[3]);

    // GL_SetOrtho caches whether it has already run; the viewport it cached
    // against is not necessarily the one we just put back. This is the engine's
    // own way of saying "assume nothing".
    GL_SetOrthoScale(1.0f);

    if (depth_was)   dglEnable(GL_DEPTH_TEST);
    if (blend_was)   dglEnable(GL_BLEND);
    if (scissor_was) dglEnable(GL_SCISSOR_TEST);
    if (cull_was)    dglEnable(GL_CULL_FACE);
}

void shader::post_scene_blur(float amount)
{
    scene_blur_ = amount;
}

void shader::post_invalidate()
{
    destroy_targets_();
}


bool shader::gbuffer_wanted()
{
    // Only while the three-attachment framebuffer is actually bound. Writing
    // outFragment1 and outFragment2 with nowhere to put them is undefined, and
    // the world is drawn from more places than this file can see.
    return active_mrt_;
}
