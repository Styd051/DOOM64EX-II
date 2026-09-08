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
// DESCRIPTION: Shader source preprocessing.
//
// GLSL has no #include, so shader sources have to be flattened before they
// reach the driver. That is nearly all this does -- the driver's own
// preprocessor handles macro expansion, including token pasting, which
// tools/glslpp_test confirms it supports.
//
// Conditionals are the exception. progs/common.inc picks its backend with
// #if defined(__ORBIS__) / #elif defined(HLSL) / #else, and picks its globals
// with #ifdef SHADER_VERTEX / #ifdef SHADER_PIXEL. Following every branch would
// pull in files meant for other backends, one of which is not even shipped. So
// conditionals are evaluated here too -- but only to decide which #include to
// follow. The directives themselves are passed through untouched, and the
// driver remains the authority on which code compiles.
//
//-----------------------------------------------------------------------------

#ifndef __SHADER_PREPROCESSOR__41908822
#define __SHADER_PREPROCESSOR__41908822

#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include <prelude.hh>

namespace imp {
  namespace shader {
    /*!
     * Which half of a .shader file to build. A single source holds both, gated
     * on SHADER_VERTEX and SHADER_PIXEL, and is preprocessed once per stage.
     */
    enum struct Stage {
        vertex,
        pixel
    };

    StringView to_string(Stage stage);

    /*! Thrown for a missing include, an include cycle, or a malformed directive. */
    struct error : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /*! Where a line of flattened output came from. */
    struct Location {
        String file;        //!< device path, or "<prologue>" for injected lines
        size_t line {};     //!< 1-based line within that file
    };

    /*!
     * A flattened shader source, plus the mapping needed to point driver
     * diagnostics back at the file the offending line actually came from.
     */
    class Source {
        String m_text;
        Vector<Location> m_lines;

        friend class Preprocessor;

        void m_push(StringView text, Location loc);

    public:
        /*! Source text, ready for glShaderSource. */
        const String& text() const
        { return m_text; }

        size_t line_count() const
        { return m_lines.size(); }

        /*!
         * Resolve a line of the flattened source back to its origin.
         * @param line 1-based line in text()
         * @return Origin, or an empty file name if the line is out of range
         */
        Location locate(size_t line) const;

        /*! Format locate() as "progs/common_glsl.inc:47". */
        String describe(size_t line) const;

        /*!
         * Rewrite the line numbers in a driver info log so they name the
         * original file. Understands the "0(123)" form NVIDIA emits and the
         * "0:123" form Mesa and AMD emit. Anything unrecognised is passed
         * through unchanged.
         */
        String translate_log(StringView log) const;
    };

    /*!
     * Flattens a shader source tree read from the loaded wad devices.
     *
     * Sources are addressed by device path through wad::open_path, not by lump
     * name: progs/common.inc and progs/common_glsl.inc share the lump name
     * "COMMON".
     */
    class Preprocessor {
        struct Macro {
            bool has_value {};
            long value {};
        };

        std::unordered_map<String, Macro> m_macros;
        std::unordered_set<String> m_pragma_once;
        std::unordered_map<String, String> m_injections;
        /* Which injections have already fired in the current process() run.
         * Per run, not permanent: the same Preprocessor flattens two stages of
         * two programs, and every one of them needs the override spliced in. */
        std::unordered_set<String> m_injected;
        Vector<String> m_include_stack;
        Vector<String> m_prologue;
        size_t m_max_depth { 32 };
        size_t m_fragment_outputs { 1 };
        int m_glsl_version { 330 };

        Source* m_out {};

        void m_include(const String& path, size_t depth);
        bool m_eval(StringView expr, const String& file, size_t line) const;

    public:
        /*! Define a macro with no value, as `-DNAME` would. */
        Preprocessor& define(StringView name);

        /*! Define a macro with an integer value, visible to #if evaluation. */
        Preprocessor& define(StringView name, long value);

        /*! Define a macro with a textual value. Not visible to #if evaluation. */
        Preprocessor& define(StringView name, StringView value);

        /*! GLSL version for the #version line and the GLSL_VERSION macro. Default 330. */
        Preprocessor& glsl_version(int version)
        { m_glsl_version = version; return *this; }

        /*!
         * Splice a second file in right after a given include resolves.
         *
         * This is what lets a KEX shader be adapted without editing it: its
         * own progs/common.inc defines outReturn, and an override spliced
         * immediately after can redefine it -- which is how the alpha test
         * becomes a discard without a line of doomSceneMain changing.
         *
         * @param after Include path to watch for, e.g. "progs/common.inc"
         * @param inject Path to splice in behind it
         */
        Preprocessor& inject_after(StringView after, StringView inject);

        /*! Maximum #include nesting before giving up. Default 32. */
        Preprocessor& max_depth(size_t depth)
        { m_max_depth = depth; return *this; }

        /*!
         * How many fragment outputs to declare for the pixel stage. Default 1.
         *
         * On the OpenGL path common_glsl.inc leaves def_var_fragment and
         * def_var_pixelTarget empty -- only the Vulkan path declares anything --
         * yet the shaders still assign to outFragment0..N. The declarations have
         * to come from here. Shaders that write a G-buffer want 3.
         */
        Preprocessor& fragment_outputs(size_t count)
        { m_fragment_outputs = count; return *this; }

        /*!
         * Flatten a shader for one stage.
         *
         * @param path Device path, e.g. "progs/default.shader"
         * @param stage Which half to build
         * @return Flattened source with its line map
         * @throws error if a file is missing, or includes form a cycle
         */
        Source process(StringView path, Stage stage);
    };

    /*! Register the `shaderdump` console command. */
    void init_commands();
  }
}

#endif //__SHADER_PREPROCESSOR__41908822
