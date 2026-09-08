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

#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat2 float2x2
#define mat3 float3x3
#define mat4 float4x4
#define ivec2 int2
#define ivec3 int3
#define ivec4 int4
#define bvec2 bool2
#define bvec3 bool3
#define bvec4 bool4

#define rcp(x) (1.0 / (x))

#define flatten [flatten]
#define branch [branch]
#define unroll [unroll]

#define flat

#define cast(type, n) (type)(n)

#define fract(x) ((x) - floor((x)))
#define mix(x, y, a) ((x)*(1.0-(a))+(y)*(a))
#define dFdx(x) ddx(x)
#define dFdy(y) ddy(y)

#define vec2op(v, op, val) (v op val)
#define vec3op(v, op, val) (v op val)
#define internalconst const 

#define inversesqrt(x) rsqrt(x)

#define begin_input(name)                           struct name {
#define end_input                                   };

#define begin_output(name)                          struct name {
#define end_output                                  };

#define uniform_buffer(name, block)                 cbuffer name

#define hlslAttribName1(idx)                        attrib##idx
#define hlslAttribName2(idx)                        ATTRIB##idx

#define var_attrib(idx, type)                       type hlslAttribName1(idx) : hlslAttribName2(idx)
#define inVarAttrib(idx, structName)                structName.hlslAttribName1(idx)

#define def_var_out(type, name, gpuName)            type name : gpuName;
#define def_var_in(type, name, gpuName)             type name : gpuName;

#define def_flat_var_out(type, name, gpuName)       type name : gpuName;
#define def_flat_var_in(type, name, gpuName)        type name : gpuName;

#define def_var_outPosition(name)                   def_var_in(vec4, name, SV_POSITION)
#define def_var_position(name)                      def_var_out(vec4, name, SV_POSITION)
#define def_var_fragment(name)                      def_var_out(vec4, name, SV_TARGET)
#define def_var_pixelTarget(type, idx)              type outPixelTarget##idx : SV_Target##idx;

#define shader_main(outStruct, inStruct, inName)    outStruct main(inStruct inName)
#define shader_pixel(outStruct, inStruct, inName)   outStruct main(float4 inFragCoord : SV_Position, inStruct inName)

#define declareOutVar(outStruct, name)              outStruct name;
#define outVar(name, field)                         name.field
#define outVarPosition(name, field)                 name.field
#define outVarFragment(name, field)                 name.field
#define outVarPixelTarget(name, idx)                name.outPixelTarget##idx
#define inVar(inName, field)                        inName.field
#define outReturn(name)                             return name;

#define def_sampler(dim, name, idx)   \
    Texture##dim uTex_##name : register(t##idx); \
    SamplerState uSample##name : register(s##idx)
    
#define def_samplerArray(dim, name, idx)    \
    Texture##dim##Array uTex_##name : register(t##idx);   \
    SamplerState uSample##name : register(s##idx)

#define sampleLevelZero(name, coord)                uTex_##name.SampleLevel (uSample##name, coord, 0)
#define sample(name, coord)                         uTex_##name.Sample (uSample##name, coord)
#define sampleLODOffset(name, coord, lod, offset)   uTex_##name.SampleLevel(uSample##name, coord, lod, offset)
#define load(name, coord, mip)                      uTex_##name.Load (ivec3(coord, mip))
#define loadArray(name, coord, layer, mip)          uTex_##name.Load (ivec4(coord, layer, mip))

#define M(mtx, col, row)                            mtx[row][col]
#define MV4(mtx, col)                               vec4(mtx[0][col], mtx[1][col], mtx[2][col], mtx[3][col])
#define MV3(mtx, col)                               vec3(mtx[0][col], mtx[1][col], mtx[2][col])
#define MV2(mtx, col)                               vec2(mtx[0][col], mtx[1][col])
