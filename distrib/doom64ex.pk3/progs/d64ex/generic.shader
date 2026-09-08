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
// Everything that is not world geometry: the HUD, the console, the menus, the
// automap, the sky, the screen melt.
//
// These bind a plain 2D texture and lean on fixed-function state that KEX's
// shaders know nothing about -- the texture environment (GL_ADD for the melt
// and the sky, GL_REPLACE when lights are off) and the engine's own fog. That
// is precisely why progs/doomSceneMain.shader cannot serve them, and why this
// file will outlive progs/d64ex/world.shader.
//
//-----------------------------------------------------------------------------

#include "progs/common.inc"

#ifdef SHADER_VERTEX

//----------------------------------------------------
// input
begin_input(inVertex)
    var_attrib(ATTRIB_POSITION, vec3);
    var_attrib(ATTRIB_TEXCOORD, vec2);
    var_attrib(ATTRIB_COLOR, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2,  out_texcoord, TEXCOORD0)
    def_var_out(vec4,  out_color,    COLOR0)
    def_var_out(float, out_fogz,     TEXCOORD1)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)

    vec4 vertex = vec4(inVarAttrib(ATTRIB_POSITION, input), 1.0);
    vec4 eye    = mul(uModelViewMatrix, vertex);

    outVarPosition(output, position) = mul(uProjectionMatrix, eye);
    outVar(output, out_texcoord)     = inVarAttrib(ATTRIB_TEXCOORD, input);
    outVar(output, out_color)        = inVarAttrib(ATTRIB_COLOR, input);

    // Distance from the eye, the way fixed-function fog measures it.
    outVar(output, out_fogz) = -eye.z;

    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2,  out_texcoord, TEXCOORD0)
    def_var_in(vec4,  out_color,    COLOR0)
    def_var_in(float, out_fogz,     TEXCOORD1)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tBase, 0);

// Texture environment of unit 0: 0 modulate, 1 add, 2 replace,
// 3 no texture at all -- the immediate-mode paths disable GL_TEXTURE_2D and
// expect the vertex colour on its own.
uniform float uTexMode;

// The sector light the second texture unit used to add.
uniform vec3 uSectorLight;

// x: 0 off, 1 GL_LINEAR, 2 GL_EXP
// y: fog start (linear) or density (exp)
// z: fog end (linear)
uniform vec3 uFogParams;
uniform vec3 uFogColor;

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)

    vec4 texel  = sampleLevelZero(tBase, inVar(input, out_texcoord));
    vec4 vcolor = inVar(input, out_color);

    vec4 final;
    if (uTexMode < 0.5) {
        // GL_MODULATE
        final = texel * vcolor;
    } else if (uTexMode < 1.5) {
        // GL_ADD: colour adds, alpha still multiplies
        final = vec4(texel.rgb + vcolor.rgb, texel.a * vcolor.a);
    } else if (uTexMode < 2.5) {
        // GL_REPLACE
        final = texel;
    } else {
        // Texturing off
        final = vcolor;
    }

    // GL_ADD on the second unit touched RGB only; alpha stayed as it was.
    final.rgb += uSectorLight;

    if (uFogParams.x > 0.5) {
        float z = inVar(input, out_fogz);
        float f;

        if (uFogParams.x < 1.5) {
            // GL_LINEAR: (end - z) / (end - start)
            f = (uFogParams.z - z) / max(uFogParams.z - uFogParams.y, 0.0001);
        } else {
            // GL_EXP: exp(-density * z)
            f = exp(-uFogParams.y * z);
        }

        final.rgb = mix(uFogColor, final.rgb, saturate(f));
    }

    outVarFragment(output, fragment) = final;

    outReturn(output)
}

#endif
