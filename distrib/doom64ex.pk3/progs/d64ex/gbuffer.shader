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
// Brings the off-screen buffer back to the window, and shows what is in it.
//
// Two jobs, because they are the same draw. Normally it copies the colour
// target across, which is what has to happen when the G-buffer is being written
// but no post-process effect is selected. With uShowMode set it displays one of
// the other targets instead -- the only way to see whether doomSceneMain's
// depth and velocity outputs hold anything sensible, since nothing consumes
// them yet.
//
//-----------------------------------------------------------------------------

#include "progs/common.inc"

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(ATTRIB_POSITION, vec3);
    var_attrib(ATTRIB_TEXCOORD, vec2);
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

    vec4 vertex = vec4(inVarAttrib(ATTRIB_POSITION, input), 1.0);

    outVarPosition(output, position) = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)     = inVarAttrib(ATTRIB_TEXCOORD, input);

    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

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

def_sampler(2D, tBase, 0);
def_sampler(2D, tDepth, 1);
def_sampler(2D, tVelocity, 2);

// 0 colour, 1 depth, 2 velocity, 3 the mask doomSceneMain puts in alpha
uniform float uShowMode;

// Environmental brightness: KEX's second brightness slider, r_brightness,
// applied where they apply it -- to the world, before the interface is drawn
// over it. 1 is neutral, the range is 0 to 2.
//
// It ADDS. Kaiser's own words for it, in the article of 24 March 2020 listing
// what separates EX 2.5 from the remaster: "It applies an additive layer over
// the screen to increase the brightness even further."
//
// This was a multiply until that sentence was read, and the difference is not
// cosmetic: a multiply leaves black black, an addition lifts it. Which is
// exactly what the side-by-side comparisons kept showing in the dark corners of
// MAP17 without anyone being able to name it.
//
// Only the plain colour path uses it. A depth or velocity view is a measurement,
// and shifting a measurement by a display preference would make it lie.
uniform float uEnvBrightness;

// Display gamma, as an exponent. 1 is identity.
//
// Applied in 0-255 space, not 0-1: the engine's own curve is pow(c, 1 + gamma/100)
// over byte values, where an exponent above 1 brightens. The same exponent on a
// normalised colour would darken instead -- 0.39^1.14 is 0.34, while 100^1.14 is
// 188, which is 0.74. Same number, opposite effect.
uniform float uGamma;

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)

    vec2 uv = inVar(input, out_texcoord);
    vec4 result;

    if (uShowMode < 0.5) {
        result = sampleLevelZero(tBase, uv);
        result.rgb = saturate(result.rgb + (uEnvBrightness - 1.0));
        result.rgb = saturate(pow(result.rgb * 255.0, vec3(uGamma, uGamma, uGamma)) / 255.0);
    }
    else if (uShowMode < 1.5) {
        // Already normalised against uZFar, so it is shown as it is. Doubled
        // only because a Doom 64 room fills a small part of that range.
        float d = saturate(sampleLevelZero(tDepth, uv).r * 2.0);
        result = vec4(d, d, d, 1.0);
    }
    else if (uShowMode < 2.5) {
        // PackVelocity maps [-1,1] to [0,1] through a cube, so a still frame
        // sits at 0.5. Recentre it: grey means no motion, red and green are the
        // two axes.
        vec2 v = UnpackVelocity(sampleLevelZero(tVelocity, uv).xy);
        result = vec4(v * 0.5 + 0.5, 0.5, 1.0);
    }
    else if (uShowMode < 3.5) {
        float m = sampleLevelZero(tVelocity, uv).a;
        result = vec4(m, m, m, 1.0);
    }
    else {
        // sao.shader writes vec4(occlusion, depth, 0, 0): the occlusion lives in
        // red alone. Splatting it across the three channels is what makes it both
        // readable on screen and correct as a multiply over the scene.
        float o = sampleLevelZero(tBase, uv).r;
        result = vec4(o, o, o, 1.0);
    }

    outVarFragment(output, fragment) = result;

    outReturn(output)
}

#endif
