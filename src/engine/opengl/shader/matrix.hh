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
// DESCRIPTION: A stand-in for the fixed-function matrix stack.
//
// The core profile has no glMatrixMode, no glRotatef, no matrix stack at all,
// and the shaders currently read their matrices back out of it with glGetFloatv.
// Both of those have to go before the context can be switched.
//
// So this mirrors the stack in plain C++. The dgl* matrix macros feed it
// alongside the real one, which means it can be checked against the thing it
// replaces while that thing still exists -- matrix_selftest reports any
// divergence. Once it is trusted, the GL calls come out and this stays.
//
// Column-major, same as OpenGL, so the arrays hand straight to
// glUniformMatrix4fv.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_MATRIX__61208843
#define __SHADER_MATRIX__61208843

#include <prelude.hh>

namespace imp {
  namespace shader {
    enum struct MatrixKind {
        projection,
        modelview
    };

    /*! Which stack the operations below act on. Mirrors glMatrixMode. */
    void matrix_mode(unsigned gl_mode);

    void matrix_load_identity();
    void matrix_load(const float* m);
    void matrix_mult(const float* m);
    void matrix_push();
    void matrix_pop();

    void matrix_rotate(float angle, float x, float y, float z);
    void matrix_translate(float x, float y, float z);
    void matrix_scale(float x, float y, float z);
    void matrix_ortho(double left, double right, double bottom, double top,
                      double znear, double zfar);

    // Double-precision forms, for the dgl* variants that take them.
    void matrix_load_d(const double* m);
    void matrix_mult_d(const double* m);
    void matrix_frustum(double left, double right, double bottom, double top,
                        double znear, double zfar);

    /*! Current matrix, column-major, 16 floats. */
    const float* matrix_get(MatrixKind kind);

    /*!
     * Compare both stacks against what OpenGL holds and report the worst
     * difference. Only meaningful while the fixed-function stack still exists.
     *
     * @return true if both match within tolerance
     */
    bool matrix_selftest();
  }
}

#endif //__SHADER_MATRIX__61208843
