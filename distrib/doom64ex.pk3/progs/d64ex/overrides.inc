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
// Spliced in right behind progs/common.inc by the engine's preprocessor, so
// that a KEX shader can be adapted without a line of it changing.
//
// The one thing that needs adapting is the alpha test. GL_ALPHA_TEST does not
// exist in a core profile, and KEX's shaders never had to think about it -- they
// were written for a renderer that had already dropped it. Every shader ends the
// same way though:
//
//     outVarFragment(output, fragment) = ...;   // writes outFragment0
//     outReturn(output)                         // return;
//
// so redefining outReturn puts the test exactly where it belongs: after the
// fragment is written, where a discard still cancels it.
//
//-----------------------------------------------------------------------------

#pragma once

#ifdef SHADER_PIXEL

// x: 0 off, 1 on
// y: reference value the fragment's alpha is compared against
uniform vec2 uAlphaTest;

#undef outReturn
#define outReturn(name)                                     \
    {                                                       \
        if (uAlphaTest.x > 0.5 && outFragment0.a < uAlphaTest.y) \
            discard;                                        \
        return;                                             \
    }

#endif
