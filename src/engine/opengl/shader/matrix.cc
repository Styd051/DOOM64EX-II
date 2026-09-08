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
#include <cstring>
#include <algorithm>

#include "doomtype.h"
#include "doomdef.h"

#include "shader/matrix.hh"
#include "shader/gl33.hh"
#include "gl_main.h"

using namespace imp;
using namespace imp::shader;

namespace {
  constexpr size_t STACK_DEPTH = 32;

  struct Stack {
      float m[STACK_DEPTH][16];
      size_t top {};

      Stack()
      { identity(); }

      float* current()
      { return m[top]; }

      void identity()
      {
          float* c = current();
          for (size_t i {}; i < 16; ++i)
              c[i] = (i % 5 == 0) ? 1.0f : 0.0f;
      }

      void push()
      {
          if (top + 1 >= STACK_DEPTH)
              return;

          std::memcpy(m[top + 1], m[top], sizeof(float) * 16);
          ++top;
      }

      void pop()
      {
          if (top > 0)
              --top;
      }
  };

  Stack projection_;
  Stack modelview_;
  Stack* active_ = &modelview_;

  /*!
   * current = current * b, column-major, matching glMultMatrixf.
   */
  void mult_(float* out, const float* a, const float* b)
  {
      float r[16];

      for (int c = 0; c < 4; ++c) {
          for (int row = 0; row < 4; ++row) {
              r[c * 4 + row] = a[0 * 4 + row] * b[c * 4 + 0]
                             + a[1 * 4 + row] * b[c * 4 + 1]
                             + a[2 * 4 + row] * b[c * 4 + 2]
                             + a[3 * 4 + row] * b[c * 4 + 3];
          }
      }

      std::memcpy(out, r, sizeof(r));
  }
}

void shader::matrix_mode(unsigned gl_mode)
{
    active_ = (gl_mode == GL_PROJECTION) ? &projection_ : &modelview_;
}

void shader::matrix_load_identity()
{
    active_->identity();
}

void shader::matrix_load(const float* m)
{
    std::memcpy(active_->current(), m, sizeof(float) * 16);
}

void shader::matrix_mult(const float* m)
{
    mult_(active_->current(), active_->current(), m);
}

void shader::matrix_push()
{
    active_->push();
}

void shader::matrix_pop()
{
    active_->pop();
}

void shader::matrix_rotate(float angle, float x, float y, float z)
{
    float len = std::sqrt(x * x + y * y + z * z);
    if (len == 0.0f)
        return;

    x /= len;
    y /= len;
    z /= len;

    float rad = angle * static_cast<float>(M_PI) / 180.0f;
    float c = std::cos(rad);
    float s = std::sin(rad);
    float t = 1.0f - c;

    // Column-major, as glRotatef builds it.
    float m[16] = {
        t * x * x + c,      t * x * y + s * z,  t * x * z - s * y,  0.0f,
        t * x * y - s * z,  t * y * y + c,      t * y * z + s * x,  0.0f,
        t * x * z + s * y,  t * y * z - s * x,  t * z * z + c,      0.0f,
        0.0f,               0.0f,               0.0f,               1.0f
    };

    matrix_mult(m);
}

void shader::matrix_translate(float x, float y, float z)
{
    float m[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        x,    y,    z,    1.0f
    };

    matrix_mult(m);
}

void shader::matrix_scale(float x, float y, float z)
{
    float m[16] = {
        x,    0.0f, 0.0f, 0.0f,
        0.0f, y,    0.0f, 0.0f,
        0.0f, 0.0f, z,    0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    matrix_mult(m);
}

void shader::matrix_ortho(double left, double right, double bottom, double top,
                          double znear, double zfar)
{
    double rl = right - left;
    double tb = top - bottom;
    double fn = zfar - znear;

    if (rl == 0.0 || tb == 0.0 || fn == 0.0)
        return;

    float m[16] = {
        static_cast<float>(2.0 / rl), 0.0f, 0.0f, 0.0f,
        0.0f, static_cast<float>(2.0 / tb), 0.0f, 0.0f,
        0.0f, 0.0f, static_cast<float>(-2.0 / fn), 0.0f,
        static_cast<float>(-(right + left) / rl),
        static_cast<float>(-(top + bottom) / tb),
        static_cast<float>(-(zfar + znear) / fn),
        1.0f
    };

    matrix_mult(m);
}

void shader::matrix_load_d(const double* m)
{
    float f[16];
    for (size_t i {}; i < 16; ++i)
        f[i] = static_cast<float>(m[i]);

    matrix_load(f);
}

void shader::matrix_mult_d(const double* m)
{
    float f[16];
    for (size_t i {}; i < 16; ++i)
        f[i] = static_cast<float>(m[i]);

    matrix_mult(f);
}

void shader::matrix_frustum(double left, double right, double bottom, double top,
                            double znear, double zfar)
{
    double rl = right - left;
    double tb = top - bottom;
    double fn = zfar - znear;

    if (rl == 0.0 || tb == 0.0 || fn == 0.0)
        return;

    float m[16] = {
        static_cast<float>((2.0 * znear) / rl), 0.0f, 0.0f, 0.0f,
        0.0f, static_cast<float>((2.0 * znear) / tb), 0.0f, 0.0f,
        static_cast<float>((right + left) / rl),
        static_cast<float>((top + bottom) / tb),
        static_cast<float>(-(zfar + znear) / fn),
        -1.0f,
        0.0f, 0.0f, static_cast<float>(-(2.0 * zfar * znear) / fn), 0.0f
    };

    matrix_mult(m);
}

const float* shader::matrix_get(MatrixKind kind)
{
    return (kind == MatrixKind::projection)
           ? projection_.current()
           : modelview_.current();
}

bool shader::matrix_selftest()
{
    // Nothing left to compare against.
    //
    // This walked the engine's stack against OpenGL's after every frame, and it
    // is what proved the replacement correct: the mirrors were built alongside
    // the fixed-function calls and validated against them, one divergence at a
    // time. That is how dglTranslated turned up missing, and why r_clipper's
    // dglGetDoublev was found reading matrices straight from GL.
    //
    // The fixed-function half is gone now -- D64_FF expands to nothing and glad
    // no longer declares glLoadMatrixf -- so GL's stack sits at identity and a
    // comparison against it would only measure its own absence.
    //
    // Kept as a function because -mtxcheck still calls it, and because the day
    // someone adds a second matrix path this is where it gets checked again.
    return true;
}

