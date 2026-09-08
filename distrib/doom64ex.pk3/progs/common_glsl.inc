//
// Copyright(C) 2007-2014 Samuel Villarreal
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

#pragma once

#define mul(m1, m2) (m1 * m2)
#define static
#define saturate(a) clamp(a, 0.0, 1.0)
#define cast(type, n) type(n)
#define fmod(x, y) mod(x, y)
#define unroll
#define inline
#define frac(x) fract(x)
#define flatten
#define branch

#define rcp(x) (1.0 / (x))

#define vec2op(v, op, val) (v.x op val && v.y op val)
#define vec3op(v, op, val) (v.x op val && v.y op val && v.z op val)
    
#ifndef VULKAN
    #define internalconst
#else
    #define internalconst const 
#endif

#if GLSL_VERSION >= 330
    #define glslAttrib(idx, type)                   layout(location = idx) in type attrib##idx
#elif GLSL_VERSION >= 130
    #define glslAttrib(idx, type)                   in type attrib##idx
#else
    #define glslAttrib(idx, type)                   attribute type attrib##idx
#endif

#define glslAttribName(idx)                         attrib##idx

#define begin_input(name)
#define end_input

#define begin_output(name)
#define end_output

#ifdef VULKAN
    #define cbuffer_binding                         __binding__
#else
    #define cbuffer_binding                         std140
#endif

#define uniform_buffer(name, block)                 layout (cbuffer_binding) uniform name

#define var_attrib(idx, type)                       glslAttrib(idx, type)
#define inVarAttrib(idx, structName)                glslAttribName(idx)

#ifdef VULKAN
    #define glsl_out        __out__
    #define glsl_in         __in__
#else
    #if GLSL_VERSION < 130
        #define glsl_out    varying
        #define glsl_in     varying
    #else
        #define glsl_out    out
        #define glsl_in     in
    #endif
#endif

#define def_var_out(type, name, gpuName)            glsl_out type name;
#define def_var_in(type, name, gpuName)             glsl_in type name;

#define def_flat_var_out(type, name, gpuName)       flat glsl_out type name;
#define def_flat_var_in(type, name, gpuName)        flat glsl_in type name;

#define def_var_outPosition(name)
#define def_var_position(name)

#ifdef VULKAN
    #define def_var_fragment(name)                  layout (location = 0) out vec4 outFragment0;
    #define def_var_pixelTarget(type, idx)          layout (location = idx) out vec4 outFragment##idx;
#else
    #define def_var_fragment(name)
    #define def_var_pixelTarget(type, idx)
#endif

#define shader_main(outStruct, inStruct, inName)    void main()
#define shader_pixel(outStruct, inStruct, inName)   void main()

#define declareOutVar(outStruct, name)
#define outVar(name, field)                         field
#define outVarPosition(name, field)                 gl_Position

#define outVarFragment(name, field)                 outFragment0
#define outVarPixelTarget(name, idx)                outFragment##idx

#define inVar(inName, field)                        field
#define outReturn(name)                             return;

#ifdef VULKAN
    #define def_sampler(dim, name, idx)             layout (__binding__) uniform sampler##dim uTex_##name
    #define def_samplerArray(dim, name, idx)        layout (__binding__) uniform sampler##dim##Array uTex_##name
#else
    #define def_sampler(dim, name, idx)             uniform sampler##dim uTex_##name
    #define def_samplerArray(dim, name, idx)        uniform sampler##dim##Array uTex_##name
#endif

#define sampleLevelZero(name, coord)                texture(uTex_##name, coord)
#define sample(name, coord)                         texture(uTex_##name, coord)
#define sampleLODOffset(name, coord, lod, offset)   textureLodOffset(uTex_##name, coord, float(lod), offset)
#define load(name, coord, mip)                      texelFetch(uTex_##name, coord, mip)
#define loadArray(name, coord, layer, mip)          texelFetch(uTex_##name, ivec3(coord, layer), mip)

#define inFragCoord                                 gl_FragCoord

#define M(mtx, col, row)                            mtx[col][row]
#define MV4(mtx, col)                               mtx[col]
#define MV3(mtx, col)                               mtx[col].xyz
#define MV2(mtx, col)                               mtx[col].xy
