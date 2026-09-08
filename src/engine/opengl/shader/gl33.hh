// -*- mode: c++ -*-
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
// DESCRIPTION: What is left of the GL 3.3 shim.
//
// This file used to declare fifty-one modern entry points and resolve them one
// by one through SDL, because glad had been generated for 1.4 and knew nothing
// past it. Glad is now generated for 3.3 core and declares all of them itself,
// so every glSomething became a plain glSomething and the shim shrank to
// three functions and two typedefs.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_GL33__70314885
#define __SHADER_GL33__70314885

// Only the GL types are wanted here, not gl_main.h -- that header names
// dboolean, so it drags in the Doom headers. Defining APIENTRY first keeps
// glad from pulling in windows.h, exactly as gl_main.h does.
#if _WIN32
#define APIENTRY __stdcall
#endif
#include "glad/glad.h"

namespace imp {
  namespace gl33 {
    /*
     * Pointer-sized, and now correct at the source.
     *
     * The old glad was generated with --omit-khrplatform, which left GLsizeiptr
     * as `long` -- 32 bits under MSVC on x64, while the driver entry point takes
     * a pointer-sized argument. A buffer size passed that way left the top half
     * of the register undefined and glBufferData answered GL_INVALID_VALUE on a
     * perfectly ordinary size. The regenerated header carries khrplatform, so
     * these are simply glad's own types; the aliases stay because the call sites
     * read better for them.
     */
    using sizeiptr = GLsizeiptr;
    using intptr = GLintptr;

    /*!
     * Kept so the call sites need not change: glad's own loader now runs in
     * GL_Init and there is nothing left here to resolve.
     * @return true
     */
    bool load();

    /*! Whether glad has loaded. */
    bool loaded();

    /*!
     * Whether the current context is a core profile.
     *
     * The engine only asks for core now, so this answers true in every ordinary
     * run. It survives because -gl14 can still ask SDL for a compatibility
     * context, and because a driver is free to hand back something else than
     * what was asked for.
     */
    bool core_profile();
  }
}

#endif //__SHADER_GL33__70314885
