//
// Copyright(C) 2015-2019 Samuel Villarreal
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
    var_attrib(3, int);
    var_attrib(4, int);
    var_attrib(5, int);
    var_attrib(6, vec4);
end_input

//----------------------------------------------------
// output
begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2,     out_texcoord,     TEXCOORD0)
    def_var_out(vec4,     out_color,        COLOR0)
    def_flat_var_out(int, out_offset,       OFFSET1)
    def_flat_var_out(int, out_dimensions,   DIMS)
    def_flat_var_out(int, out_options,      OPTIONS)
    def_var_out(vec4,     out_flash,        COLOR1)
    def_var_out(vec3,     out_position,     POSITION0)
#if HAS_MRT == 1
    def_var_out(vec4,     out_curPosition,  POSITION1)
    def_var_out(vec4,     out_prevPosition, POSITION2)
#endif
end_output

//----------------------------------------------------
shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)
    
    vec4 inPos                          = vec4(inVarAttrib(0, input), 1.0);
    vec4 vertex                         = mul(uModelViewMatrix, inPos);
    outVarPosition(output, position)    = mul(uProjectionMatrix, vertex);
    outVar(output, out_texcoord)        = inVarAttrib(1, input);
    outVar(output, out_color)           = inVarAttrib(2, input);
    outVar(output, out_offset)          = inVarAttrib(3, input);
    outVar(output, out_dimensions)      = inVarAttrib(4, input);
    outVar(output, out_options)         = inVarAttrib(5, input);
    outVar(output, out_flash)           = inVarAttrib(6, input);
    outVar(output, out_position)        = vertex.xyz;
#if HAS_MRT == 1
    outVar(output, out_curPosition)     = outVarPosition(output, position);
    outVar(output, out_prevPosition)    = mul(uPrevProjection, mul(uPrevModelView, inPos));
#endif    

    outReturn(output)
}

#endif

#ifdef SHADER_PIXEL

//----------------------------------------------------
// input
begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2,      out_texcoord,      TEXCOORD0)
    def_var_in(vec4,      out_color,         COLOR0)
    def_flat_var_in(int,  out_offset,        OFFSET1)
    def_flat_var_in(int,  out_dimensions,    DIMS)
    def_flat_var_in(int,  out_options,       OPTIONS)
    def_var_in(vec4,      out_flash,         COLOR1)
    def_var_in(vec3,      out_position,      POSITION0)
#if HAS_MRT == 1
    def_var_in(vec4,      out_curPosition,   POSITION1)
    def_var_in(vec4,      out_prevPosition,  POSITION2)
#endif
end_input

//----------------------------------------------------
// output
begin_output(outPixel)
#if HAS_MRT == 1
    def_var_pixelTarget(vec4, 0)
    def_var_pixelTarget(vec4, 1)
    def_var_pixelTarget(vec4, 2)
#else
    def_var_fragment(fragment)
#endif
end_output

def_samplerArray(2D, tBase, 0);

#define PAGE_SIZE   1024

begin_cbuffer(PostProcess_DoomFog, 15)
    cbuffer_member(vec3,    uFogColor);
    cbuffer_member(float,   uFogFar);
    cbuffer_member(float,   uFogNear);
end_cbuffer()

//----------------------------------------------------
// Converts linear coordinates to texture coordinates
ivec2 GetRemappedCoordinates(const vec2 coords, const int width, const int height, const int offset)
{
    internalconst int startOffset = offset;
    ivec2 iTC = ivec2(coords * vec2(float(width), float(height)));
    
    int tOffset = startOffset + ((iTC.y % height) * width) + (iTC.x % width);
    
    int y_newOffset = (tOffset / PAGE_SIZE) & (PAGE_SIZE-1);
    int x_newOffset = (tOffset & (PAGE_SIZE-1));
    
    return ivec2(x_newOffset, y_newOffset);
}

//----------------------------------------------------
shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)
    
    // ---------------------------------------------------------------------------------
    // unpack all the properties stored in the vertex
    
    // try nudging by (0.5 / 255.0) texels to round off and avoid z-fighting
    internalconst int layer     = int((inVar(input, out_flash).a+0.00196) * 255.0);
    internalconst int width     = (inVar(input, out_dimensions) >> 16) & 0xFFFF;
    internalconst int height    = inVar(input, out_dimensions) & 0xFFFF;
    internalconst int flipX     = (inVar(input, out_options) >> 1) & 0x1;
    internalconst int flipY     = inVar(input, out_options) & 0x1;
    internalconst int offset    = inVar(input, out_offset);
    internalconst int glow      = (inVar(input, out_options) >> 3) & 0xFF;
    
    float w     = float(width);
    float h     = float(height);
    
    internalconst float texelx = rcp(w);
    internalconst float texely = rcp(h);
    
    vec2 vFlip = vec2(float(flipX), float(flipY));
    
    vec4 final;
    
#if USE_FILTERING == 1
    vec2 vTCA = vec2(texelx, 0.0);      // texel coordinate A
    vec2 vTCB = vec2(0.0, texely);      // texel coordinate B
#endif

    vec2 vTCC = vec2(texelx, texely);   // texel coordinate C
    
    vec2 length = vec2(1.0, 1.0);
    
    // ---------------------------------------------------------------------------------
    // handle texture repeats. mirroring is also taken into account
    vec2 uvCoord = inVar(input, out_texcoord);
    uvCoord += ((length - abs((fract(uvCoord * 0.5) * 2.0) - length)) - uvCoord) * vFlip;
    
    // nudging by one-quater of a texel fixes some seam issues
#if USE_FILTERING == 1
    // center by one-quater of a texel to account for the filter offset
    vec2 fTC    = (uvCoord + (vTCC * 0.25)) - vTCC;
#else
    vec2 fTC    = uvCoord - (vTCC * 0.25);
#endif
    
#if USE_FILTERING == 1
    // ---------------------------------------------------------------------------------
    // emulate N64's 3-point linear filter
    vTCA += fTC;
    vTCB += fTC;
    vTCC += fTC;
    
    vTCA += ((length - abs((fract(vTCA * 0.5) * 2.0) - length)) - vTCA) * vFlip;
    vTCB += ((length - abs((fract(vTCB * 0.5) * 2.0) - length)) - vTCB) * vFlip;
    vTCC += ((length - abs((fract(vTCC * 0.5) * 2.0) - length)) - vTCC) * vFlip;
    
    ivec2 tc1 = GetRemappedCoordinates(fTC, width, height, offset);
    ivec2 tc2 = GetRemappedCoordinates(vTCA, width, height, offset);
    ivec2 tc3 = GetRemappedCoordinates(vTCB, width, height, offset);
    ivec2 tc4 = GetRemappedCoordinates(vTCC, width, height, offset);
    
    vec4 p0q0 = loadArray(tBase, tc1, layer, 0);
    vec4 p1q0 = loadArray(tBase, tc2, layer, 0);
    vec4 p0q1 = loadArray(tBase, tc3, layer, 0);
    vec4 p1q1 = loadArray(tBase, tc4, layer, 0);
    
#if 1
    // ---------------------------------------------------------------------------------
    // more expensive, but better looking
    float interp_x = modf(fTC.x * w, w);
	float interp_y = modf(fTC.y * h, h);

	if (fTC.x < 0)
	{
		interp_x = 1-interp_x*(-1);
	}
	if (fTC.y < 0)
	{
		interp_y = 1-interp_y*(-1);
	}
	
    final = p0q0;
    
	final = (final + interp_x *
            (p1q0 - final) + interp_y *
            (p0q1 - final)) *
            (1-step(1, interp_x + interp_y));
            
	final += (p1q1 + (1-interp_x) *
             (p0q1 - p1q1) + (1-interp_y) *
             (p1q0 - p1q1)) *
             step(1, interp_x + interp_y);
             
#else
    // ---------------------------------------------------------------------------------
    // lower quality but faster
    float a = fract(fTC.x * float(width)); 
    float b = fract(fTC.y * float(height));
    
    vec4 pInterp_q0 = mix(p0q0, p1q0, a);
    vec4 pInterp_q1 = mix(p0q1, p1q1, a);
    
    final = mix(pInterp_q0, pInterp_q1, b);
#endif
#else
    final = loadArray(tBase, GetRemappedCoordinates(fTC, width, height, offset), layer, 0);
#endif

    // ---------------------------------------------------------------------------------
    // apply sector light level glows
    final.rgb += float(glow) / 255.0;
    final *= inVar(input, out_color);
    
    // ---------------------------------------------------------------------------------
    // apply player hud flashes
    final.rgb += inVar(input, out_flash).rgb;
    
    // ---------------------------------------------------------------------------------
    // apply fog
    float depth = -(inVar(input, out_position).z / uZFar);
    float alpha = final.a;
    final = mix(vec4(uFogColor, 1.0), final, 1.0 - smoothstep(uFogNear, uFogFar, depth));
    
#if HAS_MRT == 1
    // ---------------------------------------------------------------------------------
    // compute velocity
    vec4 pos1 = inVar(input, out_curPosition);
    vec4 pos2 = inVar(input, out_prevPosition);
    
    vec2 a = pos1.xy / pos1.w;
    vec2 b = pos2.xy / pos2.w;
    
#if HLSL || VULKAN
    vec2 v = vec2(a.x - b.x, b.y - a.y);
#else
    vec2 v = a - b;
#endif

    float fMask = step(0.99, alpha);
    
    outVarPixelTarget(output, 0) = vec4(final.rgb, alpha);
    outVarPixelTarget(output, 1) = vec4(depth, depth, depth, fMask);
    outVarPixelTarget(output, 2) = vec4(PackVelocity(v), 0.0, fMask);
#else
    outVarFragment(output, fragment) = vec4(final.rgb, alpha);
#endif
    outReturn(output)
}

#endif
