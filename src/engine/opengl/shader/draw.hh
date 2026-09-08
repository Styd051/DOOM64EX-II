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
// DESCRIPTION: Drawing vtx_t batches through the programmable pipeline.
//
// dglSetVertex and dglDrawGeometry are the single funnel every triangle in the
// engine passes through, so that is where the programmable path plugs in. With
// r_Shaders off nothing changes; with it on, the same vertices go to
// progs/default.shader instead of the fixed pipeline.
//
// The projection and modelview matrices are read back out of the fixed pipeline
// with glGetFloatv rather than being recomputed. The context is a compatibility
// one, so they are still there, and taking them verbatim means the two paths
// can be compared by flicking a cvar.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_DRAW__58201477
#define __SHADER_DRAW__58201477

// vtx_t. Like gl_main.h itself, this has to follow the Doom headers -- it names
// dboolean without including anything.
#include "gl_main.h"
#include "shader/preprocessor.hh"

namespace imp {
  namespace shader {
    /*! Build default.shader and its buffers. Called from shader::init(). */
    void init_draw();

    /*!
     * Apply the settings the renderer builds its shaders with, so that a dump
     * or a test compile sees exactly what the renderer sees.
     */
    void configure_preprocessor(Preprocessor& pp);

    /*! Whether draw_geometry may be called. */
    bool draw_ready();

    /*!
     * Sector light level for the batches that follow, as DL_ProcessDrawList
     * hands it to the second texture unit.
     *
     * @param params The vtxlist_t params field, 0..255
     */
    void set_sector_light(int params);

    /*!
     * Stamp one batch's atlas metadata onto its vertices, at the point where
     * the fixed pipeline would have bound a texture.
     *
     * The values are per-batch, not per-vertex, so nothing that builds geometry
     * has to know about the array texture -- only the one place that already
     * knows which texture the batch uses.
     *
     * @param verts Batch vertices
     * @param count How many
     * @param offset Linear texel offset within the layer
     * @param layer Array layer
     * @param width Texture width
     * @param height Texture height
     * @param mirror_s Mirrored repeat horizontally
     * @param mirror_t Mirrored repeat vertically
     * @param glow Sector light, 0..255
     */
    void stamp_batch(vtx_t* verts, size_t count,
                     unsigned offset, unsigned layer,
                     unsigned width, unsigned height,
                     bool mirror_s, bool mirror_t, unsigned glow);

    /*!
     * Draw an indexed triangle batch the way the fixed pipeline would have:
     * texture times vertex colour, under the current GL matrices.
     *
     * @param verts Vertex array, as handed to dglSetVertex
     * @param vert_count Number of vertices
     * @param indices Triangle indices
     * @param index_count Number of indices
     * @param mode Primitive, GL_TRIANGLES or GL_LINES
     */
    /*!
     * Mark the end of the world for this frame.
     *
     * The matrices it was drawn with become uPrevProjection and uPrevModelView
     * for the next one, which is what doomSceneMain differences to produce the
     * velocity target.
     */
    void world_frame_done();

    /*!
     * The projection the world was last drawn with.
     *
     * uInvFocalCoords is derived from it: SAO reconstructs an eye-space
     * position from a depth, which needs the field of view.
     */
    const float* world_projection();

    /*! The far plane depth is normalised against. See WORLD_FAR in draw.cc. */
    float world_far();

    /*!
     * The distances the world fog runs between, in world units.
     *
     * Stated by the renderer rather than read back out of the fixed-pipeline fog
     * mirror. The engine's own fog was an exponential fitted to the N64's linear
     * ramp with a magic constant, and deriving a near and a far from that
     * exponential put the near at zero -- fog starting at the camera. The
     * original's own numbers go straight to doomSceneMain instead.
     *
     * @param near_units Where fog begins
     * @param far_units  Where it is complete
     */
    void set_world_fog(float near_units, float far_units);

    /*! Whether the world should write the three-target G-buffer this frame. */
    bool gbuffer_wanted();

    void draw_geometry(const vtx_t* verts, size_t vert_count,
                       const unsigned short* indices, size_t index_count,
                       unsigned mode = GL_TRIANGLES);
  }
}

#endif //__SHADER_DRAW__58201477
