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
#include <cstring>
#include <algorithm>

// These come first: gl_main.h, reached through shader/gl33.hh, names dboolean
// without including anything, and g_actions.h names int64 and event_t.
#include "doomtype.h"
#include "doomdef.h"
#include "d_event.h"
#include "g_actions.h"
#include "m_misc.h"

#include "shader/program.hh"
#include "shader/uniformblock.hh"
#include "shader/gl33.hh"
#include "shader/atlas.hh"
#include "shader/draw.hh"
#include "shader/postprocess.hh"
#include "wad/wad.hh"

using namespace imp;
using namespace imp::shader;


extern cvar::BoolVar r_n64filter;
extern cvar::BoolVar gl_useuniformbuffers;

namespace {
  String shader_log_(GLuint id)
  {
      GLint len {};
      glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
      if (len <= 1)
          return {};

      String log(static_cast<size_t>(len), '\0');
      glGetShaderInfoLog(id, len, nullptr, &log[0]);
      log.resize(std::strlen(log.c_str()));
      return log;
  }

  String program_log_(GLuint id)
  {
      GLint len {};
      glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
      if (len <= 1)
          return {};

      String log(static_cast<size_t>(len), '\0');
      glGetProgramInfoLog(id, len, nullptr, &log[0]);
      log.resize(std::strlen(log.c_str()));
      return log;
  }

  /*!
   * Compile one stage. On failure the driver's log is rewritten through the
   * source's line map before being thrown.
   */
  GLuint compile_(const Source& src, Stage stage, StringView path)
  {
      GLuint id = glCreateShader(stage == Stage::vertex ? GL_VERTEX_SHADER
                                                              : GL_FRAGMENT_SHADER);
      if (!id)
          throw error { fmt::format("{}: glCreateShader failed", path) };

      const GLchar* text = src.text().c_str();
      GLint length = static_cast<GLint>(src.text().size());
      glShaderSource(id, 1, &text, &length);
      glCompileShader(id);

      GLint status {};
      glGetShaderiv(id, GL_COMPILE_STATUS, &status);

      String log = shader_log_(id);

      if (!status) {
          glDeleteShader(id);
          throw error { fmt::format("{} [{}] failed to compile:\n{}",
                                    path, to_string(stage),
                                    src.translate_log(log)) };
      }

      if (!log.empty()) {
          log::warn("{} [{}]:\n{}", path, to_string(stage),
                    src.translate_log(log));
      }

      return id;
  }
}

Program::~Program()
{
    reset();
}

Program::Program(Program&& other):
    m_id(other.m_id),
    m_path(std::move(other.m_path)),
    m_uniforms(std::move(other.m_uniforms))
{
    other.m_id = 0;
}

Program& Program::operator=(Program&& other)
{
    if (this != &other) {
        reset();
        m_id = other.m_id;
        m_path = std::move(other.m_path);
        m_uniforms = std::move(other.m_uniforms);
        other.m_id = 0;
    }
    return *this;
}

void Program::reset()
{
    if (m_id && gl33::loaded())
        glDeleteProgram(m_id);

    m_id = 0;
    m_uniforms.clear();
}

Program Program::build(StringView path, Preprocessor& pp)
{
    if (!gl33::loaded())
        throw error { "GL 3.3 entry points are not loaded" };

    Source vert = pp.process(path, Stage::vertex);
    Source frag = pp.process(path, Stage::pixel);

    GLuint vs = compile_(vert, Stage::vertex, path);
    GLuint fs {};

    try {
        fs = compile_(frag, Stage::pixel, path);
    } catch (...) {
        glDeleteShader(vs);
        throw;
    }

    GLuint id = glCreateProgram();
    if (!id) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw error { fmt::format("{}: glCreateProgram failed", path) };
    }

    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);

    // The program holds its own reference now.
    glDetachShader(id, vs);
    glDetachShader(id, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status {};
    glGetProgramiv(id, GL_LINK_STATUS, &status);

    String log = program_log_(id);

    if (!status) {
        glDeleteProgram(id);
        throw error { fmt::format("{} failed to link:\n{}", path, log) };
    }

    if (!log.empty())
        log::warn("{} linked with messages:\n{}", path, log);

    Program program;
    program.m_id = id;
    program.m_path = path.to_string();
    return program;
}

Program Program::build(StringView path)
{
    Preprocessor pp;
    return build(path, pp);
}

void Program::use() const
{
    glUseProgram(m_id);
}

int Program::uniform(StringView name) const
{
    String key = name.to_string();

    auto it = m_uniforms.find(key);
    if (it != m_uniforms.end())
        return it->second;

    // A member of a uniform block has no location of its own -- GL answers -1
    // for it -- so it is answered with an encoded offset instead, and the
    // set_* helpers in uniformblock.hh decode it. Returns -1 when uniform
    // buffers are off, which is every name, and this costs one map lookup.
    int loc = ublock_location(name);

    if (loc == -1)
        loc = glGetUniformLocation(m_id, key.c_str());

    m_uniforms.emplace(key, loc);
    return loc;
}

bool Program::bind_uniform_block(StringView name, unsigned binding) const
{
    GLuint index = glGetUniformBlockIndex(m_id, name.to_string().c_str());
    if (index == GL_INVALID_INDEX)
        return false;

    glUniformBlockBinding(m_id, index, binding);
    return true;
}

Vector<String> Program::active_uniforms() const
{
    GLint count {};
    GLint maxlen {};
    glGetProgramiv(m_id, GL_ACTIVE_UNIFORMS, &count);
    glGetProgramiv(m_id, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxlen);

    Vector<String> names;
    if (maxlen <= 0)
        return names;

    String buf(static_cast<size_t>(maxlen) + 1, '\0');

    for (GLint i {}; i < count; ++i) {
        GLsizei len {};
        GLint size {};
        GLenum type {};
        glGetActiveUniform(m_id, static_cast<GLuint>(i), maxlen,
                                 &len, &size, &type, &buf[0]);
        names.emplace_back(buf.c_str(), static_cast<size_t>(len));
    }

    std::sort(names.begin(), names.end());
    return names;
}

Vector<String> Program::active_uniform_blocks() const
{
    GLint count {};
    GLint maxlen {};
    glGetProgramiv(m_id, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    glGetProgramiv(m_id, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxlen);

    Vector<String> names;
    if (maxlen <= 0)
        return names;

    String buf(static_cast<size_t>(maxlen) + 1, '\0');

    for (GLint i {}; i < count; ++i) {
        GLsizei len {};
        glGetActiveUniformBlockName(m_id, static_cast<GLuint>(i), maxlen,
                                          &len, &buf[0]);
        names.emplace_back(buf.c_str(), static_cast<size_t>(len));
    }

    std::sort(names.begin(), names.end());
    return names;
}

//
// CMD_ShaderBuild
//
// Preprocesses, compiles and links a shader, then reports what the linker
// kept. "all" walks every progs/*.shader in the pk3.
//
//   shaderbuild progs/default.shader
//   shaderbuild all
//

namespace {
  /*! Everything up to the first newline, for one-line failure reports. */
  String first_line_(const String& s)
  {
      size_t nl = s.find('\n');
      return nl == String::npos ? s : s.substr(0, nl);
  }

  Vector<String> all_shaders_()
  {
      Vector<String> paths;

      for (auto& p : wad::list_paths("progs/"_sv)) {
          if (p.size() > 7 && p.compare(p.size() - 7, 7, ".shader") == 0)
              paths.push_back(p);
      }

      std::sort(paths.begin(), paths.end());
      return paths;
    }

  void build_all_()
  {
      auto paths = all_shaders_();

      size_t ok {};
      Vector<String> failures;

      for (const auto& p : paths) {
          try {
              Program prog = Program::build(p);
              I_Printf("  ok    %-42s %i uniform(s), %i block(s)\n",
                       p.c_str(),
                       static_cast<int>(prog.active_uniforms().size()),
                       static_cast<int>(prog.active_uniform_blocks().size()));
              ++ok;
          } catch (const std::exception& e) {
              I_Printf("  FAIL  %-42s %s\n", p.c_str(), first_line_(e.what()).c_str());
              failures.push_back(p);
          }
      }

      I_Printf("\n  %i of %i shaders built.\n",
               static_cast<int>(ok), static_cast<int>(paths.size()));

      if (!failures.empty()) {
          I_Printf("  Run 'shaderbuild <path>' on a failure for the full log.\n");
      }
  }
}

static CMD(ShaderBuild)
{
    if (!param[0]) {
        I_Printf("usage: shaderbuild <path>|all\n");
        I_Printf("  e.g. shaderbuild progs/default.shader\n");
        return;
    }

    if (!gl33::loaded()) {
        I_Printf("shaderbuild: GL 3.3 entry points are not loaded\n");
        return;
    }

    if (String(param[0]) == "all") {
        build_all_();
        return;
    }

    try {
        Preprocessor pp;

        // Extra defines, as NAME or NAME=value. "outputs=N" is special: it sets
        // how many fragment outputs to declare, which HAS_MRT shaders need.
        for (int i = 1; param[i]; ++i) {
            String arg = param[i];
            size_t eq = arg.find('=');

            if (eq == String::npos) {
                pp.define(arg);
                continue;
            }

            String name = arg.substr(0, eq);
            String value = arg.substr(eq + 1);

            char* end {};
            long n = std::strtol(value.c_str(), &end, 0);
            bool numeric = (*end == '\0');

            if (name == "outputs" && numeric) {
                pp.fragment_outputs(static_cast<size_t>(n));
            } else if (numeric) {
                pp.define(name, n);
            } else {
                pp.define(name, StringView { value });
            }
        }

        Program prog = Program::build(param[0], pp);

        I_Printf("%s: linked, program %i\n", param[0], static_cast<int>(prog.id()));

        auto blocks = prog.active_uniform_blocks();
        if (!blocks.empty()) {
            I_Printf("  uniform blocks:\n");
            for (const auto& b : blocks)
                I_Printf("    %s\n", b.c_str());
        }

        auto uniforms = prog.active_uniforms();
        if (!uniforms.empty()) {
            I_Printf("  uniforms:\n");
            for (const auto& u : uniforms)
                I_Printf("    %s\n", u.c_str());
        }
    } catch (const std::exception& e) {
        I_Printf("shaderbuild failed:\n%s\n", e.what());
    }
}

void shader::init()
{
    if (!gl33::load()) {
        log::warn("Shader system disabled: GL 3.3 entry points unavailable");
    }

    // r_Shaders is gone. It chose between the programmable path and the fixed
    // one, and there is no fixed one left -- a cvar that answers to nothing is
    // worse than no cvar at all, because setting it looks like it should work.
    cvar::Register()
        (r_n64filter, "r_N64Filter", "Emulate the N64's three-point texture filter")
        (gl_useuniformbuffers, "gl_UseUniformBuffers",
         "Enables OpenGL uniform buffers (requires restart)");

    init_commands();
    init_draw();
    init_atlas();
    init_post();
    G_AddCommand("shaderbuild", CMD_ShaderBuild, 0);

    // -shaderbuild <path>|all, for the same reason -shaderdump exists: the
    // console is out of reach until the game is up.
    int p = M_CheckParm("-shaderbuild");
    if (p && p < myargc - 1) {
        char* argv[5] {};
        size_t n {};

        for (int i = p + 1; i < myargc && n < 4; ++i) {
            if (myargv[i][0] == '-')
                break;
            argv[n++] = myargv[i];
        }

        if (argv[0])
            CMD_ShaderBuild(0, argv);
    }
}
