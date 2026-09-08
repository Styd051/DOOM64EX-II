//
// Morgan McGuire and Michael Mara, NVIDIA and Williams College, http://research.nvidia.com, http://graphics.cs.williams.edu
//
//  Open Source under the "BSD" license: http://www.opensource.org/licenses/bsd-license.php
//
//  Copyright (c) 2011-2012, NVIDIA
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification, are permitted provided
//  that the following conditions are met:
//
//  Redistributions of source code must retain the above copyright notice, this list of conditions
//  and the following disclaimer.
//  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
//  the following disclaimer in the documentation and/or other materials provided with the distribution.
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
//  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
//  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

//
// Copyright(C) 2016-2017 Samuel Villarreal
// Copyright(C) 2016 Night Dive Studios, Inc.
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

// personal notes: While this looks better than SSAO, I just don't see how this
// is actually faster. The time it takes to generate mipmap chains and process
// this shader is actually slower than SSAO at full resolution. Even using
// the lowest mip map has made little to no improvement on performance. On 1920x1080,
// this shader consumes ~20ms of rendering time!!!!!
//
// Maybe I am doing something wrong on the engine's end but I don't see what else
// that I should be doing other than working with a simple depth buffer.

#include "progs/common.inc"

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(0, vec3);
    var_attrib(1, vec2);
    var_attrib(2, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(0, input), 1.0);
    outVarPosition(output, position)    = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

def_sampler(2D, tDepth, 0);

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

#define LOG_MAX_OFFSET      3
#define MAX_MIP_LEVEL       5
#define NUM_SAMPLES         11
#define NUM_SPIRAL_TURNS    7

static const float InvSamples = (1.0 / NUM_SAMPLES);
static const float SpiralTurnRadians = (NUM_SPIRAL_TURNS * 6.28);

begin_cbuffer(PostProcess_SAO, 7)
    cbuffer_member(vec4,    uParams1);
    cbuffer_member(vec4,    uParams2);
    cbuffer_member(vec4,    uTextureSizes[MAX_MIP_LEVEL]);
end_cbuffer()

//----------------------------------------------------
vec2 TapLocation(int sampleNumber, float spinAngle, out float ssR)
{
    // Radius relative to ssR
    float alpha = float(sampleNumber + 0.5) * InvSamples;
    float angle = alpha * SpiralTurnRadians + spinAngle;

    ssR = alpha;
    return vec2(cos(angle), sin(angle));
}

//----------------------------------------------------
// NOTE: for whatever reason, using mip maps has little to
// no significant performance boost at all. I even saw that
// using mip maps other than 0 would actually DEGRADE performance,
// let alone giving horrendous results
//
// I really don't know anymore...
//
vec3 GetOffsetPosition(ivec2 ssC, vec2 unitOffset, float ssR)
{
    ivec2 ssP = ivec2(ssR * unitOffset) + ssC;
    int mipLevel = clamp(int(floor(log2(ssR))) - LOG_MAX_OFFSET, 0, MAX_MIP_LEVEL);

    ivec2 mipP = clamp(ssP >> mipLevel, ivec2(0, 0), ivec2(uTextureSizes[mipLevel].xy) - ivec2(1, 1));
    float depth = RGBAToDepth(load(tDepth, mipP, mipLevel));
    
    vec2 tmp = vec2(mipP) / uTextureSizes[mipLevel].xy;
    
    vec3 eyePos = UVToEyePos(uInvFocalCoords, tmp, depth * uZFar);
    eyePos.z = -eyePos.z;
    return eyePos;
}

//----------------------------------------------------
float SampleAO(in ivec2 ssC, in vec3 C, in vec3 n_C, in float ssDiskRadius,
               in int tapIndex, in float randomPatternRotationAngle, const float biasOffset)
{
    // Offset on the unit disk, spun for this pixel
    float ssR;
    vec2 unitOffset = TapLocation(tapIndex, randomPatternRotationAngle, ssR);
    ssR *= ssDiskRadius;
        
    // The occluding point in camera space
    vec3 Q = GetOffsetPosition(ssC, unitOffset, ssR);

    vec3 v = Q - C;

    float vv = dot(v, v);
    float vn = dot(v, n_C);

    const float epsilon = 0.01;
    return max((vn - (uParams1.z + biasOffset)) / (epsilon + vv), 0.0);
}

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    float occlusion = 0.0;
    
    vec2 vTCoord = inVar(input, out_texcoord);
    ivec2 iCoord = ivec2(vTCoord * uTextureSizes[0].xy);
    float depth = RGBAToDepth(load(tDepth, iCoord, 0));
    
#if 1
    float l = RGBAToDepth(load(tDepth, iCoord + ivec2(-1,  0), 0));
    float r = RGBAToDepth(load(tDepth, iCoord + ivec2( 1,  0), 0));
    float t = RGBAToDepth(load(tDepth, iCoord + ivec2( 0, -1), 0));
    float b = RGBAToDepth(load(tDepth, iCoord + ivec2( 0,  1), 0));
    
    float fddx;
    float fddy;
    
    vec2 vLR = vec2(1.0, 0.0) / uTextureSizes[0].xy;
    vec2 vTB = vec2(0.0, 1.0) / uTextureSizes[0].xy;
    
#define JAGGIES_REDUCTION_BIAS   0.001
    
#define SmallerAbsDelta(x, left, mid, right)    \
{   \
    float aa = mid - left;   \
    float bb = right - mid;  \
    x = (abs(aa+JAGGIES_REDUCTION_BIAS) < abs(bb-JAGGIES_REDUCTION_BIAS)) ? aa : bb;  \
}
    
    SmallerAbsDelta(fddx, l, depth, r);
    SmallerAbsDelta(fddy, t, depth, b);
    
#undef SmallerAbsDelta
    
    // get position (camera space)
    vec3 C = UVToEyePos(uInvFocalCoords, vTCoord, depth * uZFar);
    C.z = -C.z;
    
    vec3 R = UVToEyePos(uInvFocalCoords, vTCoord + vLR, (depth + fddx) * uZFar);
    R.z = -R.z;
    
    vec3 D = UVToEyePos(uInvFocalCoords, vTCoord + vTB, (depth + fddy) * uZFar);
    D.z = -D.z;
    
    R -= C;
    D -= C;
    
    // normals
#if !defined(GLSL_VERSION)
    vec3 n_C = -normalize(cross(R, D));
#else
    vec3 n_C = normalize(cross(R, D));
#endif
    
#else
    // get position (camera space)
    vec3 C = UVToEyePos(uInvFocalCoords, vTCoord, depth * uZFar);
    C.z = -C.z;
    
    // normals
#if !defined(GLSL_VERSION)
    vec3 n_C = -normalize(cross(dFdx(C), dFdy(C)));
#else
    vec3 n_C = -normalize(cross(dFdy(C), dFdx(C)));
#endif
#endif
    
    ivec2 ssC = ivec2(inFragCoord.xy);
    float randomPatternRotationAngle = (3 * ssC.x ^ ssC.y + ssC.x * ssC.y) * 10.0;
    float ssDiskRadius = -max((uZFar/2.0) * (-C.z / uZFar), uParams1.w) * uParams1.x / C.z;
    
    float fBiasOffset = max((depth - uParams2.x), 0.0) * (uZFar / 256.0);
    
    float sum = 0.0;
    
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 0,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 1,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 2,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 3,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 4,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 5,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 6,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 7,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 8,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 9,  randomPatternRotationAngle, fBiasOffset);
    sum += SampleAO(ssC, C, n_C, ssDiskRadius, 10, randomPatternRotationAngle, fBiasOffset);
    
    float intensity = min(max((uZFar / 256.0) * (-C.z / uZFar), 3.0), 4.0);
    float IntensityDivSamples = (uParams1.y / float(NUM_SAMPLES));
    
    float A = pow(max(0.0, 1.0 - sqrt(sum * IntensityDivSamples)), intensity);
    occlusion = smoothstep(0.2, 1.0, mix(1.0, A, saturate(ssDiskRadius - uParams2.y)));
    
    // output is RG16F. combine both occlusion results and depth for faster lookups
    // during the blurring process
    outVarFragment(output, fragment) = vec4(occlusion, depth, 0.0, 0.0);
    outReturn(output)
}

#endif
