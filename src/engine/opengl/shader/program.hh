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
// DESCRIPTION: A linked GLSL program built from a KEX-style .shader file.
//
// One source holds both stages, gated on SHADER_VERTEX and SHADER_PIXEL, so it
// is preprocessed twice and compiled twice. Driver diagnostics come back
// pointing at lines of the flattened source, which are rewritten through the
// preprocessor's line map before they reach the log -- an error reads
// "progs/common_glsl.inc:47", not "0(312)".
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_PROGRAM__33025719
#define __SHADER_PROGRAM__33025719

#include <unordered_map>

#include "shader/preprocessor.hh"

namespace imp {
  namespace shader {
    class Program {
        unsigned m_id {};
        String m_path;
        mutable std::unordered_map<String, int> m_uniforms;

    public:
        Program() = default;
        ~Program();

        Program(const Program&) = delete;
        Program& operator=(const Program&) = delete;

        Program(Program&& other);
        Program& operator=(Program&& other);

        /*!
         * Preprocess, compile and link both stages of a .shader file.
         *
         * @param path Device path, e.g. "progs/default.shader"
         * @param pp Preprocessor carrying whatever defines the variant needs
         * @return The linked program
         * @throws error on a missing include, or a compile or link failure,
         *         with driver line numbers already mapped back to their file
         */
        static Program build(StringView path, Preprocessor& pp);

        /*! Same, with a default preprocessor. */
        static Program build(StringView path);

        unsigned id() const
        { return m_id; }

        const String& path() const
        { return m_path; }

        explicit operator bool() const
        { return m_id != 0; }

        void use() const;

        /*! Uniform location, or -1. Cached, so repeated lookups are cheap. */
        int uniform(StringView name) const;

        /*!
         * Point a uniform block at a binding point.
         * @return false if the program has no such block
         */
        bool bind_uniform_block(StringView name, unsigned binding) const;

        /*! Names of the uniforms the linker kept. */
        Vector<String> active_uniforms() const;

        /*! Names of the uniform blocks the linker kept. */
        Vector<String> active_uniform_blocks() const;

        void reset();
    };

    /*!
     * Load the GL 3.3 entry points and register the shader console commands.
     * Call from GL_Init, once the context exists.
     */
    void init();
  }
}

#endif //__SHADER_PROGRAM__33025719
