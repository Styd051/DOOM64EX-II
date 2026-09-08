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
    def_var_position(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color,    COLOR0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(0, input), 1.0);
    outVarPosition(output, position)    = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    outVar(output, out_color)           = inVarAttrib(2, input);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

begin_cbuffer(ColorPickerParams, 8)
    cbuffer_member(float, uScreenX);
    cbuffer_member(float, uScreenY);
    cbuffer_member(float, uHue);
end_cbuffer()

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color,    COLOR0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

//----------------------------------------------------
vec3 HSVToRGB(const in vec3 vColor)
{
    float x = 0.0;
    float j = 0.0;
    float i = 0.0;
    int table = 0;
    float xr = 0.0;
    float xg = 0.0;
    float xb = 0.0;
    float h = vColor.r;
    float s = vColor.g;
    float v = vColor.b;

    j = h * 360.0;

    if(360.0 <= j)
    {
        j -= 360.0;
    }

    x = s;
    i = v;

    if(x != 0.0)
    {
        table = int(j / 60.0);
        float fTable = float(table);
        
        if(table < 6)
        {
            float t = (j / 60.0);
            switch(table)
            {
            case 0:
                xr = i;
                xg = ((1.0 - ((1.0 - (t - fTable)) * x)) * i);
                xb = ((1.0 - x) * i);
                break;
            case 1:
                xr = ((1.0 - (x * (t - fTable))) * i);
                xg = i;
                xb = ((1.0 - x) * i);
                break;
            case 2:
                xr = ((1.0 - x) * i);
                xg = i;
                xb = ((1.0 - ((1.0 - (t - fTable)) * x)) * i);
                break;
            case 3:
                xr = ((1.0 - x) * i);
                xg = ((1.0 - (x * (t - fTable))) * i);
                xb = i;
                break;
            case 4:
                xr = ((1.0 - ((1.0 - (t - fTable)) * x)) * i);
                xg = ((1.0 - x) * i);
                xb = i;
                break;
            case 5:
                xr = i;
                xg = ((1.0 - x) * i);
                xb = ((1.0 - (x * (t - fTable))) * i);
                break;
            default:
                return vec3(1.0, 1.0, 1.0);
            }
        }
    }
    else
    {
        xr = xg = xb = i;
    }

    return vec3(xr, xg, xb);
}

def_sampler(2D, tBase, 0);

//----------------------------------------------------
shader_pixel(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    vec2 vScreenXY = vec2(uScreenX, uScreenY) / ScreenSize();
    vec2 vScreenCoords = clamp(ScreenCoords() - vScreenXY, 0.0, 1.0);
    
#if !defined(GLSL_VERSION)
    vScreenCoords.y = 1.0 - vScreenCoords.y;
#endif
    
    vec4 vColor = vec4(uHue, vScreenCoords.x, vScreenCoords.y, 1.0);
    vColor.rgb = HSVToRGB(vColor.rgb);
    
    outVarFragment(output, fragment) = sampleLevelZero(tBase, inVar(input, out_texcoord)) * vColor;
    outReturn(output)
}

#endif
