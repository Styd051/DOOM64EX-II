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

#include <cstdio>

#include "shader/gl33.hh"

#include <prelude.hh>

bool imp::gl33::load()
{
    // Nothing to do. gladLoadGLLoader in GL_Init resolves every entry point
    // this file used to chase through SDL one name at a time.
    return true;
}

bool imp::gl33::loaded()
{
    // glad leaves a null in each pointer until its loader has run, so any one
    // of them answers the question. glCreateShader is as good as another.
    return glCreateShader != nullptr;
}

bool imp::gl33::core_profile()
{
    // -1 until asked for the first time, which must be after the context
    // exists. Every caller is downstream of GL_Init.
    static int core = -1;

    if (core < 0) {
        core = 0;

        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        int major = 0;
        int minor = 0;

        if (version && std::sscanf(version, "%d.%d", &major, &minor) == 2 &&
            (major > 3 || (major == 3 && minor >= 2))) {
            // Older drivers leave the value alone and raise GL_INVALID_ENUM, so
            // start from a value that reads as "not core".
            GLint mask = 0;
            glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
            core = (mask & GL_CONTEXT_CORE_PROFILE_BIT) ? 1 : 0;
        }

        log::info("GL profile: {} ({}.{})", core ? "core" : "compatibility", major, minor);
    }

    return core != 0;
}
