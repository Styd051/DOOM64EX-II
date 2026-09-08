//
// Copyright(C) 2016-2017 Samuel Villarreal
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
    var_attrib(ATTRIB_POSITION, vec3);
    var_attrib(ATTRIB_TEXCOORD, vec2);
    var_attrib(ATTRIB_COLOR, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color,    COLOR0)
    def_var_out(vec4, out_position, POSITION0)
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 vertex                         = vec4(inVarAttrib(ATTRIB_POSITION, input), 1.0);
    outVarPosition(output, position)    = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)        = inVarAttrib(ATTRIB_TEXCOORD, input);
    outVar(output, out_color)           = inVarAttrib(ATTRIB_COLOR, input);
    outVar(output, out_position)        = outVarPosition(output, position);
    
    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color,    COLOR0)
    def_var_in(vec4, out_position, POSITION0)
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
    def_var_fragment(fragment)
end_output

begin_cbuffer(PostProcess_Deinterleave, 10)
    cbuffer_member(int,     uEventAtFrameN);
    cbuffer_member(float,   uWidthFrac);
    cbuffer_member(float,   uHeightFrac);
end_cbuffer()

def_sampler(2D, tFrameN, 0);
def_sampler(2D, tFrameNMin1, 1);
def_sampler(2D, tFrameNMin2, 2);

static const ivec2 OnePixel = ivec2(1, 1);

//----------------------------------------------------
ivec2 CalcPixelPositionFrameN(vec2 position)
{
    return  ivec2(trunc(position.x*uWidthFrac), trunc(position.y*uHeightFrac));
}

//----------------------------------------------------
ivec2 CalcNeighborPixelPositionFrameN(ivec2 pixelPosition)
{
    ivec2 onePixel = (uEventAtFrameN == 1) ? OnePixel : -OnePixel;
    return pixelPosition + onePixel;
}

//----------------------------------------------------
inline bool IsPixelPositionInEvenColumn(int x, int y)
{
    return (x % 2) == 0 && (y % 2) == 0;
}

//----------------------------------------------------
inline bool IsPrecisePixel(float column, float row)
{
    int i = IsPixelPositionInEvenColumn(int(column), int(row)) ? 1 : 0;
    return (i ^ uEventAtFrameN) == 0;
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)

    vec2 inPos = inVar(input, out_position).xy;
#if !defined(GLSL_VERSION)
    inPos.y = uHeightFrac - inPos.y - uHeightFrac;
#endif

    vec2 pos = (inPos * 0.5 + 0.5) * (ScreenSize() / vec2(uWidthFrac, uHeightFrac));
    
    ivec2 pixelPositionFrameN = CalcPixelPositionFrameN(pos);
    
    if(IsPrecisePixel(pos.x, pos.y))
    {
        vec4 colorFrameN = load(tFrameN, pixelPositionFrameN, 0);
        
        outVarFragment(output, fragment) = colorFrameN;
        outReturn(output)
    }

    vec4 neighborColorFrameN = load(tFrameNMin1, pixelPositionFrameN, 0);
    
    outVarFragment(output, fragment) = neighborColorFrameN;
    outReturn(output)
}

#endif
