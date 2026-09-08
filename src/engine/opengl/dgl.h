// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2007-2012 Samuel Villarreal
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-----------------------------------------------------------------------------

#ifndef __DGL_H__
#define __DGL_H__

#include <math.h>

#include "gl_main.h"
#include "i_system.h"
#include "shader/matrix.hh"
#include "shader/glstate.hh"
#include "shader/immediate.hh"
#include "shader/gl33.hh"

//#define LOG_GLFUNC_CALLS
//#define USE_DEBUG_GLFUNCS

#ifdef USE_DEBUG_GLFUNCS
void dglLogError(const char *message, const char *file, int line);
#endif

#define dglGetString(name)  ((const char *)glGetString(name))

//
// D64_LEGACY
//
// Everything below that the core profile removed goes through this. The call
// happens in a compatibility context and does not in a core one, decided at
// runtime from the context itself -- so a single binary runs both ways and can
// be compared frame by frame.
//
// Runtime rather than #ifdef on purpose. A compile-time switch would have left
// the two profiles in two different binaries, which is exactly the position
// where a divergence is hardest to pin down.
//
// Nothing. The engine targets a 3.3 core context, where none of these functions
// exist -- glad itself no longer declares them, so a call site left here would
// not compile. The argument is still written out at each site because it names
// what the mirror below replaces, and that is worth reading.
#define D64_LEGACY(call)

//
// D64_FF
//
// The fixed-function half of the mirrored macros: the matrix stack, the fog,
// the alpha test, the immediate-mode colour. Each of these lines feeds both
// OpenGL and the engine's own mirror, and this is the half that stops.
//
// Nothing, like D64_LEGACY. Kept as a macro so each mirrored line still says
// which fixed-function call it stands in for -- that is the only record left of
// what the mirror replaces, and it is worth reading.
#define D64_FF(call)

//
// MATRIX STACK
//
// These feed the engine's own stack alongside the fixed-function one, so the
// replacement can be checked against what it replaces while both exist. The
// core profile removes the GL half; when that happens only the right-hand call
// disappears from each line.
//
#define dglMatrixMode(mode) (D64_FF(glMatrixMode(mode)) ::imp::shader::matrix_mode(mode))
#define dglLoadIdentity() (D64_FF(glLoadIdentity()) ::imp::shader::matrix_load_identity())
#define dglLoadMatrixf(m) (D64_FF(glLoadMatrixf(m)) ::imp::shader::matrix_load(m))
#define dglMultMatrixf(m) (D64_FF(glMultMatrixf(m)) ::imp::shader::matrix_mult(m))
#define dglPushMatrix() (D64_FF(glPushMatrix()) ::imp::shader::matrix_push())
#define dglPopMatrix() (D64_FF(glPopMatrix()) ::imp::shader::matrix_pop())
#define dglRotatef(angle, x, y, z) (D64_FF(glRotatef(angle, x, y, z)) ::imp::shader::matrix_rotate(angle, x, y, z))
#define dglTranslatef(x, y, z) (D64_FF(glTranslatef(x, y, z)) ::imp::shader::matrix_translate(x, y, z))
#define dglScalef(x, y, z) (D64_FF(glScalef(x, y, z)) ::imp::shader::matrix_scale(x, y, z))
#define dglOrtho(left, right, bottom, top, zNear, zFar) (D64_FF(glOrtho(left, right, bottom, top, zNear, zFar)) ::imp::shader::matrix_ortho(left, right, bottom, top, zNear, zFar))

//
// The double-precision variants feed the same stack. Missing dglTranslated was
// enough to make r_sky.cc's dome silently bypass it, so all of them are here.
//
#define dglTranslated(x, y, z) (D64_FF(glTranslated(x, y, z)) ::imp::shader::matrix_translate((float)(x), (float)(y), (float)(z)))
#define dglRotated(angle, x, y, z) (D64_FF(glRotated(angle, x, y, z)) ::imp::shader::matrix_rotate((float)(angle), (float)(x), (float)(y), (float)(z)))
#define dglScaled(x, y, z) (D64_FF(glScaled(x, y, z)) ::imp::shader::matrix_scale((float)(x), (float)(y), (float)(z)))
#define dglLoadMatrixd(m) (D64_FF(glLoadMatrixd(m)) ::imp::shader::matrix_load_d(m))
#define dglMultMatrixd(m) (D64_FF(glMultMatrixd(m)) ::imp::shader::matrix_mult_d(m))
#define dglFrustum(left, right, bottom, top, zNear, zFar) (D64_FF(glFrustum(left, right, bottom, top, zNear, zFar)) ::imp::shader::matrix_frustum(left, right, bottom, top, zNear, zFar))

//
// FIXED-FUNCTION STATE MIRROR
//
// Fog and alpha test are read back by the shaders, and the core profile answers
// neither. These feed a mirror alongside the real calls, so the readback can be
// dropped once state_selftest says the two agree.
//
#define dglEnable(cap) (::imp::shader::state_enable(cap, true))
#define dglDisable(cap) (::imp::shader::state_enable(cap, false))
#define dglFogf(pname, param) (D64_FF(glFogf(pname, param)) ::imp::shader::state_fog_param(pname, (float)(param)))
#define dglFogi(pname, param) (D64_FF(glFogi(pname, param)) ::imp::shader::state_fog_param(pname, (float)(param)))
#define dglFogfv(pname, params) (D64_FF(glFogfv(pname, params)) ::imp::shader::state_fog_paramv(pname, params))
#define dglAlphaFunc(func, ref) (D64_FF(glAlphaFunc(func, ref)) ::imp::shader::state_alpha_func(func, (float)(ref)))


//
// IMMEDIATE MODE
//
// Emulated rather than passed through: the core profile has none of it. What
// used to be a run of glVertex calls now gathers into a vertex array and goes
// out through dglDrawGeometryPrim, which picks the pipeline like everything
// else. See opengl/shader/immediate.cc.
//
#define dglBegin(mode) (::imp::shader::imm_begin(mode))
#define dglEnd() (::imp::shader::imm_end())
#define dglVertex3f(x, y, z) (::imp::shader::imm_vertex((float)(x), (float)(y), (float)(z)))
#define dglVertex2f(x, y) (::imp::shader::imm_vertex((float)(x), (float)(y), 0.0f))
#define dglVertex2i(x, y) (::imp::shader::imm_vertex((float)(x), (float)(y), 0.0f))
// Colour and texcoord feed both: glRectf and friends still draw with the
// current GL colour, and replacing these outright turned every one of them
// white -- the console background, the menu dim, the berserk flash.
#define dglColor4ub(r, g, b, a) (D64_FF(glColor4ub(r, g, b, a)) ::imp::shader::imm_color(r, g, b, a))
#define dglColor4ubv(v) (D64_FF(glColor4ubv(v)) ::imp::shader::imm_colorv(v))
#define dglColor4f(r, g, b, a) (D64_FF(glColor4f(r, g, b, a)) ::imp::shader::imm_colorf(r, g, b, a))
#define dglRectf(x1, y1, x2, y2) (::imp::shader::imm_rect((float)(x1), (float)(y1), (float)(x2), (float)(y2)))
#define dglRecti(x1, y1, x2, y2) (::imp::shader::imm_rect((float)(x1), (float)(y1), (float)(x2), (float)(y2)))
#define dglRects(x1, y1, x2, y2) (::imp::shader::imm_rect((float)(x1), (float)(y1), (float)(x2), (float)(y2)))
#define dglRectd(x1, y1, x2, y2) (::imp::shader::imm_rect((float)(x1), (float)(y1), (float)(x2), (float)(y2)))
#define dglTexCoord2f(s, t) (D64_FF(glTexCoord2f(s, t)) ::imp::shader::imm_texcoord((float)(s), (float)(t)))


//
// CUSTOM ROUTINES
//

void dglSetVertex(vtx_t *vtx);
void dglTriangle(int v0, int v1, int v2);
void dglDrawGeometry(dword count, vtx_t *vtx);
void dglDrawGeometryPrim(dword count, vtx_t *vtx, const word *indices,
                         dword indexcount, GLenum mode);
void dglViewFrustum(int width, int height, rfloat fovy, rfloat znear);
void dglSetVertexColor(vtx_t *v, rcolor c, word count);
void dglGetColorf(rcolor color, float* argb);
void dglTexCombReplace(void);
void dglTexCombColor(GLenum t, rcolor c, GLenum func);
void dglTexCombColorf(GLenum t, float* f, GLenum func);
void dglTexCombModulate(GLenum t, GLenum s);
void dglTexCombAdd(GLenum t, GLenum s);
void dglTexCombInterpolate(GLenum t, float a);
void dglTexCombReplaceAlpha(GLenum t);

#define dglAccum(op, value) glAccum(op, value)
// moved to the FIXED-FUNCTION STATE MIRROR block below
#define dglAreTexturesResident(n, textures, residences) glAreTexturesResident(n, textures, residences)
#define dglArrayElement(i) glArrayElement(i)
#define dglBindTexture(target, texture) glBindTexture(target, texture)
#define dglBitmap(width, height, xorig, yorig, xmove, ymove, bitmap) glBitmap(width, height, xorig, yorig, xmove, ymove, bitmap)
#define dglBlendFunc(sfactor, dfactor) glBlendFunc(sfactor, dfactor)
#define dglCallList(list) glCallList(list)
#define dglCallLists(n, type, lists) glCallLists(n, type, lists)
#define dglClear(mask) glClear(mask)
#define dglClearAccum(red, green, blue, alpha) glClearAccum(red, green, blue, alpha)
#define dglClearColor(red, green, blue, alpha) glClearColor(red, green, blue, alpha)
#define dglClearDepth(depth) glClearDepth(depth)
#define dglClearIndex(c) glClearIndex(c)
#define dglClearStencil(s) glClearStencil(s)
#define dglClipPlane(plane, equation) glClipPlane(plane, equation)
#define dglColor3b(red, green, blue) glColor3b(red, green, blue)
#define dglColor3bv(v) glColor3bv(v)
#define dglColor3d(red, green, blue) glColor3d(red, green, blue)
#define dglColor3dv(v) glColor3dv(v)
#define dglColor3f(red, green, blue) glColor3f(red, green, blue)
#define dglColor3fv(v) glColor3fv(v)
#define dglColor3i(red, green, blue) glColor3i(red, green, blue)
#define dglColor3iv(v) glColor3iv(v)
#define dglColor3s(red, green, blue) glColor3s(red, green, blue)
#define dglColor3sv(v) glColor3sv(v)
#define dglColor3ub(red, green, blue) glColor3ub(red, green, blue)
#define dglColor3ubv(v) glColor3ubv(v)
#define dglColor3ui(red, green, blue) glColor3ui(red, green, blue)
#define dglColor3uiv(v) glColor3uiv(v)
#define dglColor3us(red, green, blue) glColor3us(red, green, blue)
#define dglColor3usv(v) glColor3usv(v)
#define dglColor4b(red, green, blue, alpha) glColor4b(red, green, blue, alpha)
#define dglColor4bv(v) glColor4bv(v)
#define dglColor4d(red, green, blue, alpha) glColor4d(red, green, blue, alpha)
#define dglColor4dv(v) glColor4dv(v)
#define dglColor4fv(v) glColor4fv(v)
#define dglColor4i(red, green, blue, alpha) glColor4i(red, green, blue, alpha)
#define dglColor4iv(v) glColor4iv(v)
#define dglColor4s(red, green, blue, alpha) glColor4s(red, green, blue, alpha)
#define dglColor4sv(v) glColor4sv(v)
#define dglColor4ui(red, green, blue, alpha) glColor4ui(red, green, blue, alpha)
#define dglColor4uiv(v) glColor4uiv(v)
#define dglColor4us(red, green, blue, alpha) glColor4us(red, green, blue, alpha)
#define dglColor4usv(v) glColor4usv(v)
#define dglColorMask(red, green, blue, alpha) glColorMask(red, green, blue, alpha)
#define dglColorMaterial(face, mode) glColorMaterial(face, mode)
#define dglColorPointer(size, type, stride, pointer) D64_LEGACY(glColorPointer(size, type, stride, pointer))
#define dglCopyPixels(x, y, width, height, type) glCopyPixels(x, y, width, height, type)
#define dglCopyTexImage1D(target, level, internalFormat, x, y, width, border) glCopyTexImage1D(target, level, internalFormat, x, y, width, border)
#define dglCopyTexImage2D(target, level, internalFormat, x, y, width, height, border) glCopyTexImage2D(target, level, internalFormat, x, y, width, height, border)
#define dglCopyTexSubImage1D(target, level, xoffset, x, y, width) glCopyTexSubImage1D(target, level, xoffset, x, y, width)
#define dglCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height) glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height)
#define dglCullFace(mode) glCullFace(mode)
#define dglDeleteLists(list, range) glDeleteLists(list, range)
#define dglDeleteTextures(n, textures) glDeleteTextures(n, textures)
#define dglDepthFunc(func) glDepthFunc(func)
#define dglDepthMask(flag) glDepthMask(flag)
#define dglDepthRange(zNear, zFar) glDepthRange(zNear, zFar)
// moved to the FIXED-FUNCTION STATE MIRROR block below
#define dglDisableClientState(array) D64_LEGACY(glDisableClientState(array))
#define dglDrawArrays(mode, first, count) glDrawArrays(mode, first, count)
#define dglDrawBuffer(mode) glDrawBuffer(mode)
#define dglDrawElements(mode, count, type, indices) glDrawElements(mode, count, type, indices)
#define dglDrawPixels(width, height, format, type, pixels) glDrawPixels(width, height, format, type, pixels)
#define dglEdgeFlag(flag) glEdgeFlag(flag)
#define dglEdgeFlagPointer(stride, pointer) glEdgeFlagPointer(stride, pointer)
#define dglEdgeFlagv(flag) glEdgeFlagv(flag)
// moved to the FIXED-FUNCTION STATE MIRROR block below
#define dglEnableClientState(array) D64_LEGACY(glEnableClientState(array))
#define dglEndList() glEndList()
#define dglEvalCoord1d(u) glEvalCoord1d(u)
#define dglEvalCoord1dv(u) glEvalCoord1dv(u)
#define dglEvalCoord1f(u) glEvalCoord1f(u)
#define dglEvalCoord1fv(u) glEvalCoord1fv(u)
#define dglEvalCoord2d(u, v) glEvalCoord2d(u, v)
#define dglEvalCoord2dv(u) glEvalCoord2dv(u)
#define dglEvalCoord2f(u, v) glEvalCoord2f(u, v)
#define dglEvalCoord2fv(u) glEvalCoord2fv(u)
#define dglEvalMesh1(mode, i1, i2) glEvalMesh1(mode, i1, i2)
#define dglEvalMesh2(mode, i1, i2, j1, j2) glEvalMesh2(mode, i1, i2, j1, j2)
#define dglEvalPoint1(i) glEvalPoint1(i)
#define dglEvalPoint2(i, j) glEvalPoint2(i, j)
#define dglFeedbackBuffer(size, type, buffer) glFeedbackBuffer(size, type, buffer)
#define dglFinish() glFinish()
#define dglFlush() glFlush()
// moved to the FIXED-FUNCTION STATE MIRROR block below
// moved to the FIXED-FUNCTION STATE MIRROR block below
// moved to the FIXED-FUNCTION STATE MIRROR block below
#define dglFogiv(pname, params) glFogiv(pname, params)
#define dglFrontFace(mode) glFrontFace(mode)
// moved to the MATRIX STACK block above
#define dglGenLists(range) glGenLists(range)
#define dglGenTextures(n, textures) glGenTextures(n, textures)
#define dglGetBooleanv(pname, params) glGetBooleanv(pname, params)
#define dglGetClipPlane(plane, equation) glGetClipPlane(plane, equation)
#define dglGetDoublev(pname, params) glGetDoublev(pname, params)
#define dglGetError() glGetError()
#define dglGetFloatv(pname, params) glGetFloatv(pname, params)
#define dglGetIntegerv(pname, params) glGetIntegerv(pname, params)
#define dglGetLightfv(light, pname, params) glGetLightfv(light, pname, params)
#define dglGetLightiv(light, pname, params) glGetLightiv(light, pname, params)
#define dglGetMapdv(target, query, v) glGetMapdv(target, query, v)
#define dglGetMapfv(target, query, v) glGetMapfv(target, query, v)
#define dglGetMapiv(target, query, v) glGetMapiv(target, query, v)
#define dglGetMaterialfv(face, pname, params) glGetMaterialfv(face, pname, params)
#define dglGetMaterialiv(face, pname, params) glGetMaterialiv(face, pname, params)
#define dglGetPixelMapfv(map, values) glGetPixelMapfv(map, values)
#define dglGetPixelMapuiv(map, values) glGetPixelMapuiv(map, values)
#define dglGetPixelMapusv(map, values) glGetPixelMapusv(map, values)
#define dglGetPointerv(pname, params) glGetPointerv(pname, params)
#define dglGetPolygonStipple(mask) glGetPolygonStipple(mask)
#define dglGetTexEnvfv(target, pname, params) glGetTexEnvfv(target, pname, params)
#define dglGetTexEnviv(target, pname, params) glGetTexEnviv(target, pname, params)
#define dglGetTexGendv(coord, pname, params) glGetTexGendv(coord, pname, params)
#define dglGetTexGenfv(coord, pname, params) glGetTexGenfv(coord, pname, params)
#define dglGetTexGeniv(coord, pname, params) glGetTexGeniv(coord, pname, params)
#define dglGetTexImage(target, level, format, type, pixels) glGetTexImage(target, level, format, type, pixels)
#define dglGetTexLevelParameterfv(target, level, pname, params) glGetTexLevelParameterfv(target, level, pname, params)
#define dglGetTexLevelParameteriv(target, level, pname, params) glGetTexLevelParameteriv(target, level, pname, params)
#define dglGetTexParameterfv(target, pname, params) glGetTexParameterfv(target, pname, params)
#define dglGetTexParameteriv(target, pname, params) glGetTexParameteriv(target, pname, params)
#define dglHint(target, mode) glHint(target, mode)
#define dglIndexMask(mask) glIndexMask(mask)
#define dglIndexPointer(type, stride, pointer) glIndexPointer(type, stride, pointer)
#define dglIndexd(c) glIndexd(c)
#define dglIndexdv(c) glIndexdv(c)
#define dglIndexf(c) glIndexf(c)
#define dglIndexfv(c) glIndexfv(c)
#define dglIndexi(c) glIndexi(c)
#define dglIndexiv(c) glIndexiv(c)
#define dglIndexs(c) glIndexs(c)
#define dglIndexsv(c) glIndexsv(c)
#define dglIndexub(c) glIndexub(c)
#define dglIndexubv(c) glIndexubv(c)
#define dglInitNames() glInitNames()
#define dglInterleavedArrays(format, stride, pointer) glInterleavedArrays(format, stride, pointer)
#define dglIsEnabled(cap) glIsEnabled(cap)
#define dglIsList(list) glIsList(list)
#define dglIsTexture(texture) glIsTexture(texture)
#define dglLightModelf(pname, param) glLightModelf(pname, param)
#define dglLightModelfv(pname, params) glLightModelfv(pname, params)
#define dglLightModeli(pname, param) glLightModeli(pname, param)
#define dglLightModeliv(pname, params) glLightModeliv(pname, params)
#define dglLightf(light, pname, param) glLightf(light, pname, param)
#define dglLightfv(light, pname, params) glLightfv(light, pname, params)
#define dglLighti(light, pname, param) glLighti(light, pname, param)
#define dglLightiv(light, pname, params) glLightiv(light, pname, params)
#define dglLineStipple(factor, pattern) glLineStipple(factor, pattern)
#define dglLineWidth(width) glLineWidth(width)
#define dglListBase(base) glListBase(base)
// moved to the MATRIX STACK block above
#define dglLoadName(name) glLoadName(name)
#define dglLogicOp(opcode) glLogicOp(opcode)
#define dglMap1d(target, u1, u2, stride, order, points) glMap1d(target, u1, u2, stride, order, points)
#define dglMap1f(target, u1, u2, stride, order, points) glMap1f(target, u1, u2, stride, order, points)
#define dglMap2d(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points) glMap2d(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points)
#define dglMap2f(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points) glMap2f(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points)
#define dglMapGrid1d(un, u1, u2) glMapGrid1d(un, u1, u2)
#define dglMapGrid1f(un, u1, u2) glMapGrid1f(un, u1, u2)
#define dglMapGrid2d(un, u1, u2, vn, v1, v2) glMapGrid2d(un, u1, u2, vn, v1, v2)
#define dglMapGrid2f(un, u1, u2, vn, v1, v2) glMapGrid2f(un, u1, u2, vn, v1, v2)
#define dglMaterialf(face, pname, param) glMaterialf(face, pname, param)
#define dglMaterialfv(face, pname, params) glMaterialfv(face, pname, params)
#define dglMateriali(face, pname, param) glMateriali(face, pname, param)
#define dglMaterialiv(face, pname, params) glMaterialiv(face, pname, params)
// moved to the MATRIX STACK block above
#define dglNewList(list, mode) glNewList(list, mode)
#define dglNormal3b(nx, ny, nz) glNormal3b(nx, ny, nz)
#define dglNormal3bv(v) glNormal3bv(v)
#define dglNormal3d(nx, ny, nz) glNormal3d(nx, ny, nz)
#define dglNormal3dv(v) glNormal3dv(v)
#define dglNormal3f(nx, ny, nz) glNormal3f(nx, ny, nz)
#define dglNormal3fv(v) glNormal3fv(v)
#define dglNormal3i(nx, ny, nz) glNormal3i(nx, ny, nz)
#define dglNormal3iv(v) glNormal3iv(v)
#define dglNormal3s(nx, ny, nz) glNormal3s(nx, ny, nz)
#define dglNormal3sv(v) glNormal3sv(v)
#define dglNormalPointer(type, stride, pointer) glNormalPointer(type, stride, pointer)
#define dglPassThrough(token) glPassThrough(token)
#define dglPixelMapfv(map, mapsize, values) glPixelMapfv(map, mapsize, values)
#define dglPixelMapuiv(map, mapsize, values) glPixelMapuiv(map, mapsize, values)
#define dglPixelMapusv(map, mapsize, values) glPixelMapusv(map, mapsize, values)
#define dglPixelStoref(pname, param) glPixelStoref(pname, param)
#define dglPixelStorei(pname, param) glPixelStorei(pname, param)
#define dglPixelTransferf(pname, param) glPixelTransferf(pname, param)
#define dglPixelTransferi(pname, param) glPixelTransferi(pname, param)
#define dglPixelZoom(xfactor, yfactor) glPixelZoom(xfactor, yfactor)
#define dglPointSize(size) glPointSize(size)
#define dglPolygonMode(face, mode) (glPolygonMode(face, mode), ::imp::shader::state_set_polygon_mode(face, mode))
#define dglPolygonOffset(factor, units) glPolygonOffset(factor, units)
#define dglPolygonStipple(mask) glPolygonStipple(mask)
#define dglPopAttrib() glPopAttrib()
#define dglPopClientAttrib() glPopClientAttrib()
#define dglPopName() glPopName()
#define dglPrioritizeTextures(n, textures, priorities) glPrioritizeTextures(n, textures, priorities)
#define dglPushAttrib(mask) glPushAttrib(mask)
#define dglPushClientAttrib(mask) glPushClientAttrib(mask)
#define dglPushName(name) glPushName(name)
#define dglRasterPos2d(x, y) glRasterPos2d(x, y)
#define dglRasterPos2dv(v) glRasterPos2dv(v)
#define dglRasterPos2f(x, y) glRasterPos2f(x, y)
#define dglRasterPos2fv(v) glRasterPos2fv(v)
#define dglRasterPos2i(x, y) glRasterPos2i(x, y)
#define dglRasterPos2iv(v) glRasterPos2iv(v)
#define dglRasterPos2s(x, y) glRasterPos2s(x, y)
#define dglRasterPos2sv(v) glRasterPos2sv(v)
#define dglRasterPos3d(x, y, z) glRasterPos3d(x, y, z)
#define dglRasterPos3dv(v) glRasterPos3dv(v)
#define dglRasterPos3f(x, y, z) glRasterPos3f(x, y, z)
#define dglRasterPos3fv(v) glRasterPos3fv(v)
#define dglRasterPos3i(x, y, z) glRasterPos3i(x, y, z)
#define dglRasterPos3iv(v) glRasterPos3iv(v)
#define dglRasterPos3s(x, y, z) glRasterPos3s(x, y, z)
#define dglRasterPos3sv(v) glRasterPos3sv(v)
#define dglRasterPos4d(x, y, z, w) glRasterPos4d(x, y, z, w)
#define dglRasterPos4dv(v) glRasterPos4dv(v)
#define dglRasterPos4f(x, y, z, w) glRasterPos4f(x, y, z, w)
#define dglRasterPos4fv(v) glRasterPos4fv(v)
#define dglRasterPos4i(x, y, z, w) glRasterPos4i(x, y, z, w)
#define dglRasterPos4iv(v) glRasterPos4iv(v)
#define dglRasterPos4s(x, y, z, w) glRasterPos4s(x, y, z, w)
#define dglRasterPos4sv(v) glRasterPos4sv(v)
#define dglReadBuffer(mode) glReadBuffer(mode)
#define dglReadPixels(x, y, width, height, format, type, pixels) glReadPixels(x, y, width, height, format, type, pixels)
#define dglRectdv(v1, v2) glRectdv(v1, v2)
#define dglRectfv(v1, v2) glRectfv(v1, v2)
#define dglRectiv(v1, v2) glRectiv(v1, v2)
#define dglRectsv(v1, v2) glRectsv(v1, v2)
#define dglRenderMode(mode) glRenderMode(mode)
// moved to the MATRIX STACK block above
// moved to the MATRIX STACK block above
#define dglScissor(x, y, width, height) glScissor(x, y, width, height)
#define dglSelectBuffer(size, buffer) glSelectBuffer(size, buffer)
#define dglShadeModel(mode) D64_LEGACY(glShadeModel(mode))
#define dglStencilFunc(func, ref, mask) glStencilFunc(func, ref, mask)
#define dglStencilMask(mask) glStencilMask(mask)
#define dglStencilOp(fail, zfail, zpass) glStencilOp(fail, zfail, zpass)
#define dglTexCoord1d(s) glTexCoord1d(s)
#define dglTexCoord1dv(v) glTexCoord1dv(v)
#define dglTexCoord1f(s) glTexCoord1f(s)
#define dglTexCoord1fv(v) glTexCoord1fv(v)
#define dglTexCoord1i(s) glTexCoord1i(s)
#define dglTexCoord1iv(v) glTexCoord1iv(v)
#define dglTexCoord1s(s) glTexCoord1s(s)
#define dglTexCoord1sv(v) glTexCoord1sv(v)
#define dglTexCoord2d(s, t) glTexCoord2d(s, t)
#define dglTexCoord2dv(v) glTexCoord2dv(v)
#define dglTexCoord2fv(v) glTexCoord2fv(v)
#define dglTexCoord2i(s, t) glTexCoord2i(s, t)
#define dglTexCoord2iv(v) glTexCoord2iv(v)
#define dglTexCoord2s(s, t) glTexCoord2s(s, t)
#define dglTexCoord2sv(v) glTexCoord2sv(v)
#define dglTexCoord3d(s, t, r) glTexCoord3d(s, t, r)
#define dglTexCoord3dv(v) glTexCoord3dv(v)
#define dglTexCoord3f(s, t, r) glTexCoord3f(s, t, r)
#define dglTexCoord3fv(v) glTexCoord3fv(v)
#define dglTexCoord3i(s, t, r) glTexCoord3i(s, t, r)
#define dglTexCoord3iv(v) glTexCoord3iv(v)
#define dglTexCoord3s(s, t, r) glTexCoord3s(s, t, r)
#define dglTexCoord3sv(v) glTexCoord3sv(v)
#define dglTexCoord4d(s, t, r, q) glTexCoord4d(s, t, r, q)
#define dglTexCoord4dv(v) glTexCoord4dv(v)
#define dglTexCoord4f(s, t, r, q) glTexCoord4f(s, t, r, q)
#define dglTexCoord4fv(v) glTexCoord4fv(v)
#define dglTexCoord4i(s, t, r, q) glTexCoord4i(s, t, r, q)
#define dglTexCoord4iv(v) glTexCoord4iv(v)
#define dglTexCoord4s(s, t, r, q) glTexCoord4s(s, t, r, q)
#define dglTexCoord4sv(v) glTexCoord4sv(v)
#define dglTexCoordPointer(size, type, stride, pointer) D64_LEGACY(glTexCoordPointer(size, type, stride, pointer))
#define dglTexEnvf(target, pname, param) D64_LEGACY(glTexEnvf(target, pname, param))
#define dglTexEnvfv(target, pname, params) D64_LEGACY(glTexEnvfv(target, pname, params))
#define dglTexEnvi(target, pname, param) D64_LEGACY(glTexEnvi(target, pname, param))
#define dglTexEnviv(target, pname, params) D64_LEGACY(glTexEnviv(target, pname, params))
#define dglTexGend(coord, pname, param) glTexGend(coord, pname, param)
#define dglTexGendv(coord, pname, params) glTexGendv(coord, pname, params)
#define dglTexGenf(coord, pname, param) glTexGenf(coord, pname, param)
#define dglTexGenfv(coord, pname, params) glTexGenfv(coord, pname, params)
#define dglTexGeni(coord, pname, param) glTexGeni(coord, pname, param)
#define dglTexGeniv(coord, pname, params) glTexGeniv(coord, pname, params)
#define dglTexImage1D(target, level, internalformat, width, border, format, type, pixels) glTexImage1D(target, level, internalformat, width, border, format, type, pixels)
#define dglTexImage2D(target, level, internalformat, width, height, border, format, type, pixels) glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels)
#define dglTexParameterf(target, pname, param) glTexParameterf(target, pname, param)
#define dglTexParameterfv(target, pname, params) glTexParameterfv(target, pname, params)
#define dglTexParameteri(target, pname, param) glTexParameteri(target, pname, param)
#define dglTexParameteriv(target, pname, params) glTexParameteriv(target, pname, params)
#define dglTexSubImage1D(target, level, xoffset, width, format, type, pixels) glTexSubImage1D(target, level, xoffset, width, format, type, pixels)
#define dglTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels) glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels)
// moved to the MATRIX STACK block above
#define dglVertex2d(x, y) glVertex2d(x, y)
#define dglVertex2dv(v) glVertex2dv(v)
#define dglVertex2fv(v) glVertex2fv(v)
#define dglVertex2iv(v) glVertex2iv(v)
#define dglVertex2s(x, y) glVertex2s(x, y)
#define dglVertex2sv(v) glVertex2sv(v)
#define dglVertex3d(x, y, z) glVertex3d(x, y, z)
#define dglVertex3dv(v) glVertex3dv(v)
#define dglVertex3fv(v) glVertex3fv(v)
#define dglVertex3i(x, y, z) glVertex3i(x, y, z)
#define dglVertex3iv(v) glVertex3iv(v)
#define dglVertex3s(x, y, z) glVertex3s(x, y, z)
#define dglVertex3sv(v) glVertex3sv(v)
#define dglVertex4d(x, y, z, w) glVertex4d(x, y, z, w)
#define dglVertex4dv(v) glVertex4dv(v)
#define dglVertex4f(x, y, z, w) glVertex4f(x, y, z, w)
#define dglVertex4fv(v) glVertex4fv(v)
#define dglVertex4i(x, y, z, w) glVertex4i(x, y, z, w)
#define dglVertex4iv(v) glVertex4iv(v)
#define dglVertex4s(x, y, z, w) glVertex4s(x, y, z, w)
#define dglVertex4sv(v) glVertex4sv(v)
#define dglVertexPointer(size, type, stride, pointer) D64_LEGACY(glVertexPointer(size, type, stride, pointer))
#define dglViewport(x, y, width, height) glViewport(x, y, width, height)

//
// GL_ARB_multitexture
//

#define dglActiveTextureARB(texture) (::imp::shader::state_select_texture(texture))
#define dglClientActiveTextureARB(texture) D64_LEGACY(glClientActiveTextureARB(texture))
#define dglMultiTexCoord1dARB(target, s) glMultiTexCoord1dARB(target, s)
#define dglMultiTexCoord1dvARB(target, v) glMultiTexCoord1dvARB(target, v)
#define dglMultiTexCoord1fARB(target, s) glMultiTexCoord1fARB(target, s)
#define dglMultiTexCoord1fvARB(target, v) glMultiTexCoord1fvARB(target, v)
#define dglMultiTexCoord1iARB(target, s) glMultiTexCoord1iARB(target, s)
#define dglMultiTexCoord1ivARB(target, v) glMultiTexCoord1ivARB(target, v)
#define dglMultiTexCoord1sARB(target, s) glMultiTexCoord1sARB(target, s)
#define dglMultiTexCoord1svARB(target, v) glMultiTexCoord1svARB(target, v)
#define dglMultiTexCoord2dARB(target, s, t) glMultiTexCoord2dARB(target, s, t)
#define dglMultiTexCoord2dvARB(target, v) glMultiTexCoord2dvARB(target, v)
#define dglMultiTexCoord2fARB(target, s, t) glMultiTexCoord2fARB(target, s, t)
#define dglMultiTexCoord2fvARB(target, v) glMultiTexCoord2fvARB(target, v)
#define dglMultiTexCoord2iARB(target, s, t) glMultiTexCoord2iARB(target, s, t)
#define dglMultiTexCoord2ivARB(target, v) glMultiTexCoord2ivARB(target, v)
#define dglMultiTexCoord2sARB(target, s, t) glMultiTexCoord2sARB(target, s, t)
#define dglMultiTexCoord2svARB(target, v) glMultiTexCoord2svARB(target, v)
#define dglMultiTexCoord3dARB(target, s, t, r) glMultiTexCoord3dARB(target, s, t, r)
#define dglMultiTexCoord3dvARB(target, v) glMultiTexCoord3dvARB(target, v)
#define dglMultiTexCoord3fARB(target, s, t, r) glMultiTexCoord3fARB(target, s, t, r)
#define dglMultiTexCoord3fvARB(target, v) glMultiTexCoord3fvARB(target, v)
#define dglMultiTexCoord3iARB(target, s, t, r) glMultiTexCoord3iARB(target, s, t, r)
#define dglMultiTexCoord3ivARB(target, v) glMultiTexCoord3ivARB(target, v)
#define dglMultiTexCoord3sARB(target, s, t, r) glMultiTexCoord3sARB(target, s, t, r)
#define dglMultiTexCoord3svARB(target, v) glMultiTexCoord3svARB(target, v)
#define dglMultiTexCoord4dARB(target, s, t, r, q) glMultiTexCoord4dARB(target, s, t, r, q)
#define dglMultiTexCoord4dvARB(target, v) glMultiTexCoord4dvARB(target, v)
#define dglMultiTexCoord4fARB(target, s, t, r, q) glMultiTexCoord4fARB(target, s, t, r, q)
#define dglMultiTexCoord4fvARB(target, v) glMultiTexCoord4fvARB(target, v)
#define dglMultiTexCoord4iARB(target, s, t, r, q) glMultiTexCoord4iARB(target, s, t, r, q)
#define dglMultiTexCoord4ivARB(target, v) glMultiTexCoord4ivARB(target, v)
#define dglMultiTexCoord4sARB(target, s, t, r, q) glMultiTexCoord4sARB(target, s, t, r, q)
#define dglMultiTexCoord4svARB(target, v) glMultiTexCoord4svARB(target, v)

//
// GL_EXT_compiled_vertex_array
//

#define dglLockArraysEXT(first, count) D64_LEGACY(glLockArraysEXT(first, count))
#define dglUnlockArraysEXT() D64_LEGACY(glUnlockArraysEXT())

//
// GL_EXT_multi_draw_arrays
//

#define dglMultiDrawArraysEXT(mode, first, count, primcount) glMultiDrawArraysEXT(mode, first, count, primcount)
#define dglMultiDrawElementsEXT(mode, count, type, indices, primcount) glMultiDrawElementsEXT(mode, count, type, indices, primcount)

//
// GL_EXT_fog_coord
//

#define dglFogCoordfEXT(coord) glFogCoordfEXT(coord)
#define dglFogCoordfvEXT(coord) glFogCoordfvEXT(coord)
#define dglFogCoorddEXT(coord) glFogCoorddEXT(coord)
#define dglFogCoorddvEXT(coord) glFogCoorddvEXT(coord)
#define dglFogCoordPointerEXT(type, stride, pointer) glFogCoordPointerEXT(type, stride, pointer)

//
// GL_ARB_vertex_buffer_object
//

#define dglBindBufferARB(target, buffer) glBindBufferARB(target, buffer)
#define dglDeleteBuffersARB(n, buffers) glDeleteBuffersARB(n, buffers)
#define dglGenBuffersARB(n, buffers) glGenBuffersARB(n, buffers)
#define dglIsBufferARB(buffer) glIsBufferARB(buffer)
#define dglBufferDataARB(target, size, data, usage) glBufferDataARB(target, size, data, usage)
#define dglBufferSubDataARB(target, offset, size, data) glBufferSubDataARB(target, offset, size, data)
#define dglGetBufferSubDataARB(target, offset, size, data) glGetBufferSubDataARB(target, offset, size, data)
#define dglMapBufferARB(target, access) glMapBufferARB(target, access)
#define dglUnmapBufferARB(target) glUnmapBufferARB(target)
#define dglGetBufferParameterivARB(target, pname, params) glGetBufferParameterivARB(target, pname, params)
#define dglGetBufferPointervARB(target, pname, params) glGetBufferPointervARB(target, pname, params)

#endif // __DGL_H__

