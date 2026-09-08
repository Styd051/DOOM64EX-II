//
// Copyright(C) 2016-2019 Samuel Villarreal
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

#define ATTRIB_POSITION     0
#define ATTRIB_TEXCOORD     1
#define ATTRIB_COLOR        2
#define ATTRIB_NORMAL       3

#ifndef mad
#define mad(a, b, c) (a * b + c)
#endif

#if defined(__ORBIS__)
    #include "progs/common_pssl.inc"
#elif defined(HLSL)
    #include "progs/common_hlsl.inc"
#else
    #include "progs/common_glsl.inc"
#endif

#ifndef EMULATE_UNIFORM_BUFFERS
    #define begin_cbuffer(name, register)   \
    uniform_buffer(name, register) \
    {
        
    #define cbuffer_member(type, name)  type name
    #define end_cbuffer() };
#else
    #define begin_cbuffer(name, register)
    #define cbuffer_member(type, name) uniform type name
    #define end_cbuffer()
#endif

#include "progs/globals_structs.inc"

#ifdef SHADER_VERTEX
    #include "progs/globals_vertex.inc"
#endif

#ifdef SHADER_PIXEL
    #include "progs/globals_fragment.inc"
#endif
