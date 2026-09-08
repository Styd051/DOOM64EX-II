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

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "shader/preprocessor.hh"
#include "wad/wad.hh"

// g_actions.h is not self-contained: it names int64, dboolean, event_t and
// FILE without including anything.
#include <cstdio>
#include "doomtype.h"
#include "d_event.h"
#include "g_actions.h"
#include "m_misc.h"

// After the Doom headers: gl_main.h, reached through draw.hh, names dboolean.
#include "shader/draw.hh"

using namespace imp;
using namespace imp::shader;

namespace {
  bool is_ident_start(char c)
  { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }

  bool is_ident_char(char c)
  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

  bool is_space(char c)
  { return c == ' ' || c == '\t' || c == '\v' || c == '\f' || c == '\r'; }

  StringView ltrim(StringView s)
  {
      size_t i {};
      while (i < s.length() && is_space(s[i]))
          ++i;
      return s.substr(i);
  }

  StringView rtrim(StringView s)
  {
      size_t n = s.length();
      while (n > 0 && is_space(s[n - 1]))
          --n;
      return s.substr(0, n);
  }

  StringView trim(StringView s)
  { return rtrim(ltrim(s)); }

  /*! Leading identifier of s, empty if it doesn't start with one. */
  StringView first_word(StringView s)
  {
      size_t i {};
      while (i < s.length() && is_ident_char(s[i]))
          ++i;
      return s.substr(0, i);
  }

  /*!
   * Evaluates the subset of #if expressions the shader sources actually use:
   * integer literals, defined(), identifiers, ! && || and the comparisons.
   *
   * Unknown identifiers are 0, as in C. A macro defined without a value counts
   * as 1, which C would reject but which is the useful reading here -- it lets
   * `#if VULKAN` behave like `#ifdef VULKAN`.
   */
  class ExprParser {
      const String& m_src;
      size_t m_pos {};

      const std::function<bool(const String&)>& m_is_defined;
      const std::function<long(const String&)>& m_value_of;

      void m_skip()
      {
          while (m_pos < m_src.size() && is_space(m_src[m_pos]))
              ++m_pos;
      }

      bool m_eof()
      { m_skip(); return m_pos >= m_src.size(); }

      /*! Consume op if it's next. Two-character operators must be tried first. */
      bool m_match(const char* op)
      {
          m_skip();
          size_t n = std::strlen(op);
          if (m_src.compare(m_pos, n, op) != 0)
              return false;

          // Don't let '!' swallow the '!' of '!='.
          if (n == 1 && (op[0] == '!' || op[0] == '<' || op[0] == '>')
              && m_pos + 1 < m_src.size() && m_src[m_pos + 1] == '=')
              return false;

          m_pos += n;
          return true;
      }

      String m_read_ident()
      {
          m_skip();
          if (m_pos >= m_src.size() || !is_ident_start(m_src[m_pos]))
              throw error { fmt::format("expected an identifier at offset {}", m_pos) };

          size_t start = m_pos;
          while (m_pos < m_src.size() && is_ident_char(m_src[m_pos]))
              ++m_pos;

          return m_src.substr(start, m_pos - start);
      }

      long m_primary()
      {
          m_skip();

          if (m_pos >= m_src.size())
              throw error { "unexpected end of expression" };

          if (m_match("(")) {
              long v = m_or();
              if (!m_match(")"))
                  throw error { "missing ')' in expression" };
              return v;
          }

          if (std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) {
              char* end {};
              long v = std::strtol(m_src.c_str() + m_pos, &end, 0);
              m_pos = static_cast<size_t>(end - m_src.c_str());

              // Skip any integer suffix.
              while (m_pos < m_src.size()
                     && std::strchr("uUlL", m_src[m_pos]) != nullptr)
                  ++m_pos;

              return v;
          }

          if (is_ident_start(m_src[m_pos])) {
              String id = m_read_ident();

              if (id == "defined") {
                  bool paren = m_match("(");
                  String name = m_read_ident();
                  if (paren && !m_match(")"))
                      throw error { "missing ')' after defined()" };
                  return m_is_defined(name) ? 1 : 0;
              }

              return m_value_of(id);
          }

          throw error { fmt::format("unexpected character '{}' in expression",
                                    m_src[m_pos]) };
      }

      long m_unary()
      {
          if (m_match("!"))
              return m_unary() ? 0 : 1;
          return m_primary();
      }

      long m_rel()
      {
          long v = m_unary();
          for (;;) {
              if (m_match("<="))      v = (v <= m_unary());
              else if (m_match(">=")) v = (v >= m_unary());
              else if (m_match("<"))  v = (v <  m_unary());
              else if (m_match(">"))  v = (v >  m_unary());
              else break;
          }
          return v;
      }

      long m_eq()
      {
          long v = m_rel();
          for (;;) {
              if (m_match("=="))      v = (v == m_rel());
              else if (m_match("!=")) v = (v != m_rel());
              else break;
          }
          return v;
      }

      long m_and()
      {
          long v = m_eq();
          while (m_match("&&")) {
              long r = m_eq();
              v = (v && r);
          }
          return v;
      }

      long m_or()
      {
          long v = m_and();
          while (m_match("||")) {
              long r = m_and();
              v = (v || r);
          }
          return v;
      }

  public:
      ExprParser(const String& src,
                 const std::function<bool(const String&)>& is_defined,
                 const std::function<long(const String&)>& value_of):
          m_src(src), m_is_defined(is_defined), m_value_of(value_of) {}

      long parse()
      {
          long v = m_or();
          if (!m_eof())
              throw error { fmt::format("trailing garbage at offset {}", m_pos) };
          return v;
      }
  };

  /*! One frame of #if / #elif / #else / #endif nesting. */
  struct Cond {
      bool parent_active {};
      bool taken {};      //!< a branch has already been entered
      bool active {};     //!< this branch is the live one
  };
}

StringView shader::to_string(Stage stage)
{
    switch (stage) {
    case Stage::vertex: return "vertex"_sv;
    case Stage::pixel:  return "pixel"_sv;
    }
    return "?"_sv;
}

//
// Source
//

void Source::m_push(StringView text, Location loc)
{
    m_text.append(text.data(), text.length());
    m_text.push_back('\n');
    m_lines.push_back(std::move(loc));
}

Location Source::locate(size_t line) const
{
    if (line == 0 || line > m_lines.size())
        return {};
    return m_lines[line - 1];
}

String Source::describe(size_t line) const
{
    auto loc = locate(line);
    if (loc.file.empty())
        return fmt::format("<line {}>", line);
    return fmt::format("{}:{}", loc.file, loc.line);
}

String Source::translate_log(StringView log) const
{
    String out;
    String text = log.to_string();

    size_t i {};
    while (i < text.size()) {
        // Drivers prefix a diagnostic with the source string index and the
        // line: "0(123)" on NVIDIA, "0:123" on Mesa and AMD. Only rewrite one
        // when it sits at the start of a line or right after "ERROR: ".
        bool at_start = (i == 0) || (text[i - 1] == '\n')
                        || (i >= 2 && text.compare(i - 2, 2, ": ") == 0);

        if (at_start && std::isdigit(static_cast<unsigned char>(text[i]))) {
            size_t j = i;
            while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j])))
                ++j;

            char open = (j < text.size()) ? text[j] : '\0';
            if (open == '(' || open == ':') {
                size_t k = j + 1;
                size_t line {};
                bool any {};
                while (k < text.size() && std::isdigit(static_cast<unsigned char>(text[k]))) {
                    line = line * 10 + static_cast<size_t>(text[k] - '0');
                    ++k;
                    any = true;
                }

                if (any && line <= m_lines.size()) {
                    if (open == '(' && k < text.size() && text[k] == ')')
                        ++k;

                    out += describe(line);
                    i = k;
                    continue;
                }
            }
        }

        out.push_back(text[i]);
        ++i;
    }

    return out;
}

//
// Preprocessor
//

Preprocessor& Preprocessor::inject_after(StringView after, StringView inject)
{
    m_injections[after.to_string()] = inject.to_string();
    return *this;
}

Preprocessor& Preprocessor::define(StringView name)
{
    m_macros[name.to_string()] = Macro { false, 0 };
    m_prologue.push_back(fmt::format("#define {}", name));
    return *this;
}

Preprocessor& Preprocessor::define(StringView name, long value)
{
    m_macros[name.to_string()] = Macro { true, value };
    m_prologue.push_back(fmt::format("#define {} {}", name, value));
    return *this;
}

Preprocessor& Preprocessor::define(StringView name, StringView value)
{
    m_macros[name.to_string()] = Macro { false, 0 };
    m_prologue.push_back(fmt::format("#define {} {}", name, value));
    return *this;
}

bool Preprocessor::m_eval(StringView expr, const String& file, size_t line) const
{
    std::function<bool(const String&)> is_defined = [this](const String& name) {
        return m_macros.find(name) != m_macros.end();
    };

    std::function<long(const String&)> value_of = [this](const String& name) -> long {
        auto it = m_macros.find(name);
        if (it == m_macros.end())
            return 0;
        return it->second.has_value ? it->second.value : 1;
    };

    String text = trim(expr).to_string();
    if (text.empty())
        throw error { fmt::format("{}:{}: empty #if condition", file, line) };

    try {
        return ExprParser { text, is_defined, value_of }.parse() != 0;
    } catch (const error& e) {
        throw error { fmt::format("{}:{}: cannot evaluate '{}': {}",
                                  file, line, text, e.what()) };
    }
}

void Preprocessor::m_include(const String& path, size_t depth)
{
    if (depth > m_max_depth)
        throw error { fmt::format("#include nested deeper than {} levels, at '{}'",
                                  m_max_depth, path) };

    if (m_pragma_once.count(path))
        return;

    for (const auto& parent : m_include_stack) {
        if (parent == path)
            throw error { fmt::format("'{}' includes itself", path) };
    }

    auto lump = wad::open_path(path);
    if (!lump) {
        String from = m_include_stack.empty()
                      ? String { "<root>" }
                      : m_include_stack.back();
        throw error { fmt::format("'{}' not found (included from '{}')", path, from) };
    }

    String content = lump->read_bytes();

    m_include_stack.push_back(path);

    Vector<Cond> conds;
    auto active = [&conds] {
        return conds.empty() || conds.back().active;
    };

    size_t line_no {};
    size_t pos {};

    while (pos <= content.size()) {
        size_t eol = content.find('\n', pos);
        if (eol == String::npos)
            eol = content.size();

        StringView raw { content.data() + pos, eol - pos };
        ++line_no;

        StringView body = ltrim(raw);
        bool emit = true;

        if (!body.empty() && body[0] == '#') {
            StringView rest = ltrim(body.substr(1));
            StringView word = first_word(rest);
            StringView args = trim(rest.substr(word.length()));

            if (word == "include") {
                // Consumed either way: the driver has no #include.
                emit = false;

                if (active()) {
                    if (args.length() < 2)
                        throw error { fmt::format("{}:{}: malformed #include",
                                                  path, line_no) };

                    char open = args[0];
                    char close = (open == '<') ? '>' : '"';
                    if (open != '"' && open != '<')
                        throw error { fmt::format("{}:{}: malformed #include",
                                                  path, line_no) };

                    size_t end = args.find(close, 1);
                    if (end == StringView::npos)
                        throw error { fmt::format("{}:{}: unterminated #include",
                                                  path, line_no) };

                    m_include(args.substr(1, end - 1).to_string(), depth + 1);
                }
            } else if (word == "pragma" && first_word(args) == "once") {
                m_pragma_once.insert(path);
                emit = false;
            } else if (word == "ifdef" || word == "ifndef") {
                bool parent = active();
                bool has = m_macros.count(first_word(args).to_string()) != 0;
                bool on = parent && (word == "ifdef" ? has : !has);
                conds.push_back(Cond { parent, on, on });
            } else if (word == "if") {
                bool parent = active();
                bool on = parent && m_eval(args, path, line_no);
                conds.push_back(Cond { parent, on, on });
            } else if (word == "elif") {
                if (conds.empty())
                    throw error { fmt::format("{}:{}: #elif without #if", path, line_no) };

                auto& c = conds.back();
                bool on = c.parent_active && !c.taken && m_eval(args, path, line_no);
                c.active = on;
                c.taken = c.taken || on;
            } else if (word == "else") {
                if (conds.empty())
                    throw error { fmt::format("{}:{}: #else without #if", path, line_no) };

                auto& c = conds.back();
                c.active = c.parent_active && !c.taken;
                c.taken = true;
            } else if (word == "endif") {
                if (conds.empty())
                    throw error { fmt::format("{}:{}: #endif without #if", path, line_no) };
                conds.pop_back();
            } else if (word == "define" && active()) {
                StringView name = first_word(args);
                if (!name.empty()) {
                    StringView value = args.substr(name.length());
                    Macro m {};

                    // A function-like macro has its '(' flush against the name.
                    if (!(!value.empty() && value[0] == '(')) {
                        String v = trim(value).to_string();
                        if (!v.empty()) {
                            char* end {};
                            long parsed = std::strtol(v.c_str(), &end, 0);
                            if (*end == '\0')
                                m = Macro { true, parsed };
                        }
                    }

                    m_macros[name.to_string()] = m;
                }
            } else if (word == "undef" && active()) {
                m_macros.erase(first_word(args).to_string());
            }
        }

        if (emit)
            m_out->m_push(rtrim(raw), Location { path, line_no });

        if (eol == content.size())
            break;
        pos = eol + 1;
    }

    if (!conds.empty())
        throw error { fmt::format("'{}' ends with {} unclosed #if",
                                  path, conds.size()) };

    m_include_stack.pop_back();

    // Anything registered to follow this file goes in now, while its own
    // definitions are in scope and can be overridden.
    auto inj = m_injections.find(path);
    if (inj != m_injections.end() && m_injected.insert(path).second)
        m_include(inj->second, depth + 1);
}

Source Preprocessor::process(StringView path, Stage stage)
{
    Source out;

    m_pragma_once.clear();
    m_injected.clear();
    m_include_stack.clear();

    // The prologue is rebuilt per call, since the stage macro differs.
    auto saved_macros = m_macros;
    auto saved_prologue = m_prologue;

    Vector<String> prologue;
    prologue.push_back(fmt::format("#version {} core", m_glsl_version));
    prologue.push_back(fmt::format("#define GLSL_VERSION {}", m_glsl_version));
    m_macros["GLSL_VERSION"] = Macro { true, m_glsl_version };

    const char* stage_macro = (stage == Stage::vertex) ? "SHADER_VERTEX" : "SHADER_PIXEL";
    prologue.push_back(fmt::format("#define {}", stage_macro));
    m_macros[stage_macro] = Macro { false, 0 };

    // common_glsl.inc declares no fragment outputs on the GL path, so they are
    // injected here. See Preprocessor::fragment_outputs.
    if (stage == Stage::pixel) {
        for (size_t i {}; i < m_fragment_outputs; ++i) {
            prologue.push_back(
                fmt::format("layout(location = {}) out vec4 outFragment{};", i, i));
        }
    }

    for (const auto& line : m_prologue)
        prologue.push_back(line);

    size_t n {};
    for (const auto& line : prologue)
        out.m_push(line, Location { "<prologue>", ++n });

    m_out = &out;

    try {
        m_include(path.to_string(), 0);
    } catch (...) {
        m_out = nullptr;
        m_macros = std::move(saved_macros);
        m_prologue = std::move(saved_prologue);
        throw;
    }

    m_out = nullptr;
    m_macros = std::move(saved_macros);
    m_prologue = std::move(saved_prologue);

    return out;
}

//
// CMD_ShaderDump
//
// Flattens a shader and prints it, each line tagged with where it came from.
// This is the only way to see what the driver is actually handed.
//
//   shaderdump progs/default.shader
//   shaderdump progs/default.shader pixel
//   shaderdump progs/doomSceneMain.shader pixel brief
//

static CMD(ShaderDump)
{
    if (!param[0]) {
        I_Printf("usage: shaderdump <path> [vertex|pixel] [brief]\n");
        I_Printf("  e.g. shaderdump progs/default.shader pixel\n");
        return;
    }

    Stage stage = Stage::vertex;
    bool brief {};

    for (int i = 1; param[i]; ++i) {
        String arg = param[i];
        if (arg == "pixel" || arg == "fragment")
            stage = Stage::pixel;
        else if (arg == "vertex")
            stage = Stage::vertex;
        else if (arg == "brief")
            brief = true;
        else
            I_Printf("shaderdump: ignoring unknown argument '%s'\n", arg.c_str());
    }

    try {
        Preprocessor pp;
        configure_preprocessor(pp);
        Source src = pp.process(param[0], stage);

        I_Printf("%s [%s]: %i lines\n",
                 param[0],
                 to_string(stage).to_string().c_str(),
                 static_cast<int>(src.line_count()));

        // Which files ended up spliced in, in the order they first appear.
        Vector<String> files;
        for (size_t i = 1; i <= src.line_count(); ++i) {
            const auto& f = src.locate(i).file;
            if (files.empty() || files.back() != f) {
                bool seen {};
                for (const auto& g : files)
                    if (g == f) { seen = true; break; }
                if (!seen)
                    files.push_back(f);
            }
        }

        for (const auto& f : files)
            I_Printf("  from %s\n", f.c_str());

        if (brief)
            return;

        I_Printf("\n");

        size_t line {};
        size_t start {};
        const String& text = src.text();

        while (start < text.size()) {
            size_t eol = text.find('\n', start);
            if (eol == String::npos)
                eol = text.size();

            ++line;
            I_Printf("%4i | %-28s | %s\n",
                     static_cast<int>(line),
                     src.describe(line).c_str(),
                     text.substr(start, eol - start).c_str());

            start = eol + 1;
        }
    } catch (const std::exception& e) {
        I_Printf("shaderdump failed: %s\n", e.what());
    }
}

void shader::init_commands()
{
    G_AddCommand("shaderdump", CMD_ShaderDump, 0);

    // The console isn't reachable before the game is up, and config.cfg runs
    // long before the wad is mounted, so offer the same thing on the command
    // line: -shaderdump <path> [vertex|pixel] [brief]
    int p = M_CheckParm("-shaderdump");
    if (p && p < myargc - 1) {
        char* argv[4] {};
        size_t n {};

        for (int i = p + 1; i < myargc && n < 3; ++i) {
            if (myargv[i][0] == '-')
                break;
            argv[n++] = myargv[i];
        }

        if (argv[0])
            CMD_ShaderDump(0, argv);
    }
}
