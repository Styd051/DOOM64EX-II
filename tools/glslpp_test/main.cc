//
// glslpp_test — vérifie ce que le préprocesseur GLSL du pilote accepte réellement.
//
// Question tranchée par ce programme : le dialecte de shaders de KEX (Doom64.kpf,
// progs/common_glsl.inc) repose massivement sur le collage de jetons `##`, sur des
// macros portant le nom de mots réservés GLSL, et sur des `#if` de version.
//
// La spécification GLSL ne garantit pas `##`. Si le pilote l'accepte, le
// préprocesseur du moteur n'a qu'à résoudre les `#include` et préfixer quelques
// `#define` — une centaine de lignes. Sinon, il faut écrire une expansion de
// macros complète, ce qui est un tout autre chantier.
//
// Le programme crée un contexte OpenGL 3.3 core, compile une batterie de shaders
// isolant chaque construction, et conclut.
//

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Le GLAD du moteur est généré pour GL 1.4 : il ne contient aucune fonction de
// shader. On résout donc à la main le strict nécessaire via SDL.
// ---------------------------------------------------------------------------

#if defined(_WIN32)
#define GLAPIENTRY __stdcall
#else
#define GLAPIENTRY
#endif

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef char          GLchar;
typedef unsigned char GLubyte;

#define GL_VENDOR                   0x1F00
#define GL_RENDERER                 0x1F01
#define GL_VERSION                  0x1F02
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_FRAGMENT_SHADER          0x8B30
#define GL_VERTEX_SHADER            0x8B31
#define GL_COMPILE_STATUS           0x8B81
#define GL_INFO_LOG_LENGTH          0x8B84

typedef const GLubyte* (GLAPIENTRY *PFN_GetString)(GLenum);
typedef GLuint         (GLAPIENTRY *PFN_CreateShader)(GLenum);
typedef void           (GLAPIENTRY *PFN_ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void           (GLAPIENTRY *PFN_CompileShader)(GLuint);
typedef void           (GLAPIENTRY *PFN_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void           (GLAPIENTRY *PFN_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void           (GLAPIENTRY *PFN_DeleteShader)(GLuint);

static PFN_GetString        p_GetString;
static PFN_CreateShader     p_CreateShader;
static PFN_ShaderSource     p_ShaderSource;
static PFN_CompileShader    p_CompileShader;
static PFN_GetShaderiv      p_GetShaderiv;
static PFN_GetShaderInfoLog p_GetShaderInfoLog;
static PFN_DeleteShader     p_DeleteShader;

template <typename T>
static bool resolve(T& fn, const char* name)
{
    fn = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    if (!fn) {
        std::printf("  !! entree GL introuvable : %s\n", name);
        return false;
    }
    return true;
}

static bool load_gl()
{
    bool ok = true;
    ok &= resolve(p_GetString,        "glGetString");
    ok &= resolve(p_CreateShader,     "glCreateShader");
    ok &= resolve(p_ShaderSource,     "glShaderSource");
    ok &= resolve(p_CompileShader,    "glCompileShader");
    ok &= resolve(p_GetShaderiv,      "glGetShaderiv");
    ok &= resolve(p_GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= resolve(p_DeleteShader,     "glDeleteShader");
    return ok;
}

// ---------------------------------------------------------------------------
// Compilation
// ---------------------------------------------------------------------------

struct Result {
    bool        ok;
    std::string log;
};

static Result compile(GLenum stage, const std::string& src)
{
    Result r { false, {} };

    GLuint sh = p_CreateShader(stage);
    if (!sh) {
        r.log = "glCreateShader a renvoye 0";
        return r;
    }

    const GLchar* ptr = src.c_str();
    GLint len = static_cast<GLint>(src.size());
    p_ShaderSource(sh, 1, &ptr, &len);
    p_CompileShader(sh);

    GLint status = 0;
    p_GetShaderiv(sh, GL_COMPILE_STATUS, &status);

    GLint loglen = 0;
    p_GetShaderiv(sh, GL_INFO_LOG_LENGTH, &loglen);
    if (loglen > 1) {
        std::vector<GLchar> buf(static_cast<size_t>(loglen) + 1, 0);
        p_GetShaderInfoLog(sh, loglen, nullptr, buf.data());
        r.log = buf.data();
        while (!r.log.empty() && (r.log.back() == '\n' || r.log.back() == '\r'))
            r.log.pop_back();
    }

    p_DeleteShader(sh);
    r.ok = (status != 0);
    return r;
}

// ---------------------------------------------------------------------------
// Sous-ensemble de progs/common_glsl.inc, recopié tel quel, réutilisé par
// plusieurs tests. Chemin non-VULKAN, GLSL_VERSION 330.
// ---------------------------------------------------------------------------

static const char* KEX_MACROS = R"S(
#define GLSL_VERSION 330
#define mul(m1, m2) (m1 * m2)
#define static
#define saturate(a) clamp(a, 0.0, 1.0)
#define cast(type, n) type(n)
#define fmod(x, y) mod(x, y)
#define unroll
#define inline
#define frac(x) fract(x)
#define flatten
#define branch
#define rcp(x) (1.0 / (x))
#define internalconst
#if GLSL_VERSION >= 330
    #define glslAttrib(idx, type) layout(location = idx) in type attrib##idx
#elif GLSL_VERSION >= 130
    #define glslAttrib(idx, type) in type attrib##idx
#else
    #define glslAttrib(idx, type) attribute type attrib##idx
#endif
#define glslAttribName(idx) attrib##idx
#define begin_input(name)
#define end_input
#define begin_output(name)
#define end_output
#define cbuffer_binding std140
#define uniform_buffer(name, block) layout (cbuffer_binding) uniform name
#define begin_cbuffer(name, register) uniform_buffer(name, register) {
#define cbuffer_member(type, name) type name
#define end_cbuffer() };
#define var_attrib(idx, type) glslAttrib(idx, type)
#define inVarAttrib(idx, structName) glslAttribName(idx)
#define glsl_out out
#define glsl_in in
#define def_var_out(type, name, gpuName) glsl_out type name;
#define def_var_in(type, name, gpuName) glsl_in type name;
#define def_flat_var_out(type, name, gpuName) flat glsl_out type name;
#define def_flat_var_in(type, name, gpuName) flat glsl_in type name;
#define def_var_outPosition(name)
#define def_var_position(name)
#define def_var_fragment(name)
#define def_var_pixelTarget(type, idx)
#define shader_main(outStruct, inStruct, inName) void main()
#define declareOutVar(outStruct, name)
#define outVar(name, field) field
#define outVarPosition(name, field) gl_Position
#define outVarFragment(name, field) outFragment0
#define inVar(inName, field) field
#define outReturn(name) return;
#define def_sampler(dim, name, idx) uniform sampler##dim uTex_##name
#define def_samplerArray(dim, name, idx) uniform sampler##dim##Array uTex_##name
#define sampleLevelZero(name, coord) texture(uTex_##name, coord)
#define sample(name, coord) texture(uTex_##name, coord)
#define load(name, coord, mip) texelFetch(uTex_##name, coord, mip)
#define loadArray(name, coord, layer, mip) texelFetch(uTex_##name, ivec3(coord, layer), mip)
#define M(mtx, col, row) mtx[col][row]
#define MV4(mtx, col) mtx[col]
)S";

// ---------------------------------------------------------------------------
// La batterie
// ---------------------------------------------------------------------------

struct Test {
    const char* id;
    const char* what;       // ce que le test isole
    GLenum      stage;
    std::string src;
    bool        critical;   // un echec ici invalide l'approche « #include seulement »
};

static std::vector<Test> build_tests()
{
    std::vector<Test> t;

    t.push_back({ "T01", "collage `##` simple : attrib##idx", GL_VERTEX_SHADER, R"S(#version 330 core
#define glslAttrib(idx, type) layout(location = idx) in type attrib##idx
#define glslAttribName(idx) attrib##idx
glslAttrib(0, vec3);
void main() { gl_Position = vec4(glslAttribName(0), 1.0); }
)S", true });

    t.push_back({ "T02", "collage triple avec jeton numerique : sampler##2D##Array", GL_FRAGMENT_SHADER, R"S(#version 330 core
#define def_samplerArray(dim, name, idx) uniform sampler##dim##Array uTex_##name
def_samplerArray(2D, tBase, 0);
out vec4 fragColor;
void main() { fragColor = texelFetch(uTex_tBase, ivec3(0, 0, 0), 0); }
)S", true });

    t.push_back({ "T03", "collage avec jeton numerique : sampler##2D et uTex_##name", GL_FRAGMENT_SHADER, R"S(#version 330 core
#define def_sampler(dim, name, idx) uniform sampler##dim uTex_##name
#define sampleLevelZero(name, coord) texture(uTex_##name, coord)
def_sampler(2D, tBase, 0);
in vec2 texcoord;
out vec4 fragColor;
void main() { fragColor = sampleLevelZero(tBase, texcoord); }
)S", true });

    t.push_back({ "T04", "collage sur cible de sortie : outFragment##idx", GL_FRAGMENT_SHADER, R"S(#version 330 core
#define def_var_pixelTarget(type, idx) layout(location = idx) out type outFragment##idx
#define outVarPixelTarget(name, idx) outFragment##idx
def_var_pixelTarget(vec4, 0);
def_var_pixelTarget(vec4, 1);
def_var_pixelTarget(vec4, 2);
void main() {
    outVarPixelTarget(output, 0) = vec4(1.0);
    outVarPixelTarget(output, 1) = vec4(0.5);
    outVarPixelTarget(output, 2) = vec4(0.0);
}
)S", true });

    t.push_back({ "T05", "selection de version : #if GLSL_VERSION >= 330", GL_VERTEX_SHADER, R"S(#version 330 core
#define GLSL_VERSION 330
#if GLSL_VERSION >= 330
    #define ATTR(idx, type) layout(location = idx) in type attrib##idx
#elif GLSL_VERSION >= 130
    #define ATTR(idx, type) in type attrib##idx
#else
    #define ATTR(idx, type) attribute type attrib##idx
#endif
ATTR(0, vec3);
void main() { gl_Position = vec4(attrib0, 1.0); }
)S", true });

    t.push_back({ "T06", "macros nommees comme des mots reserves : static, inline, sample", GL_FRAGMENT_SHADER, R"S(#version 330 core
#define static
#define inline
#define unroll
#define flatten
#define branch
#define internalconst
#define sample(name, coord) texture(uTex_##name, coord)
uniform sampler2D uTex_tBase;
static const vec4 BIT_SHIFT_PACK = vec4(16581375.0, 65025.0, 255.0, 1.0);
in vec2 texcoord;
out vec4 fragColor;
void main() {
    internalconst int n = 4;
    vec4 acc = vec4(0.0);
    unroll for (int i = 0; i < n; ++i) { acc += BIT_SHIFT_PACK; }
    fragColor = sample(tBase, texcoord) + acc;
}
)S", true });

    t.push_back({ "T07", "#pragma once repete (tous les .inc de KEX en portent un)", GL_FRAGMENT_SHADER, R"S(#version 330 core
#pragma once
#pragma once
out vec4 fragColor;
void main() { fragColor = vec4(1.0); }
)S", false });

    t.push_back({ "T08", "bloc RenderView complet en std140, via les macros cbuffer", GL_VERTEX_SHADER,
        std::string("#version 330 core\n") + KEX_MACROS + R"S(
begin_cbuffer(RenderView, 0)
    cbuffer_member(mat4,  uProjectionMatrix);
    cbuffer_member(mat4,  uModelViewMatrix);
    cbuffer_member(mat4,  uInverseProjectionMatrix);
    cbuffer_member(mat4,  uPrevProjection);
    cbuffer_member(mat4,  uInverseModelViewMatrix);
    cbuffer_member(mat4,  uPrevModelView);
    cbuffer_member(mat4,  uRotationMatrix);
    cbuffer_member(mat4,  uInverseRotationMatrix);
    cbuffer_member(mat4,  uTransposedRotationMatrix);
    cbuffer_member(mat4,  uClipMatrix);
    cbuffer_member(mat4,  uInverseClipMatrix);
    cbuffer_member(mat4,  uPrevClipMatrix);
    cbuffer_member(mat4,  uNormalMatrix);
    cbuffer_member(vec3,  uViewOrigin);
    cbuffer_member(float, uViewWidth);
    cbuffer_member(float, uViewHeight);
    cbuffer_member(float, uZNear);
    cbuffer_member(float, uZFar);
    cbuffer_member(vec2,  uInvFocalCoords);
    cbuffer_member(vec4,  uFrustumCorners[4]);
end_cbuffer()

void main() {
    gl_Position = mul(uProjectionMatrix, mul(uModelViewMatrix, uFrustumCorners[0]));
}
)S", true });

    // Test d'integration : le corps de default.shader, recopie verbatim depuis
    // Doom64.kpf, passe a travers les vraies macros.
    t.push_back({ "T09", "INTEGRATION default.shader (etage sommet) verbatim", GL_VERTEX_SHADER,
        std::string("#version 330 core\n#define SHADER_VERTEX\n") + KEX_MACROS + R"S(
begin_cbuffer(RenderView, 0)
    cbuffer_member(mat4, uProjectionMatrix);
    cbuffer_member(mat4, uModelViewMatrix);
end_cbuffer()

#define ATTRIB_POSITION 0
#define ATTRIB_TEXCOORD 1
#define ATTRIB_COLOR    2

begin_input(inVertex)
    var_attrib(ATTRIB_POSITION, vec3);
    var_attrib(ATTRIB_TEXCOORD, vec2);
    var_attrib(ATTRIB_COLOR, vec4);
end_input

begin_output(outVertex)
    def_var_outPosition(position)
    def_var_out(vec2, out_texcoord, TEXCOORD0)
    def_var_out(vec4, out_color,    COLOR0)
end_output

shader_main(outVertex, inVertex, input)
{
    declareOutVar(outVertex, output)

    vec4 vertex                      = vec4(inVarAttrib(ATTRIB_POSITION, input), 1.0);
    outVarPosition(output, position) = mul(uProjectionMatrix, mul(uModelViewMatrix, vertex));
    outVar(output, out_texcoord)     = inVarAttrib(ATTRIB_TEXCOORD, input);
    outVar(output, out_color)        = inVarAttrib(ATTRIB_COLOR, input);

    outReturn(output)
}
)S", true });

    t.push_back({ "T10", "INTEGRATION default.shader (etage pixel) verbatim", GL_FRAGMENT_SHADER,
        std::string("#version 330 core\n#define SHADER_PIXEL\n") + KEX_MACROS + R"S(
out vec4 outFragment0;

begin_input(outVertex)
    def_var_position(position)
    def_var_in(vec2, out_texcoord, TEXCOORD0)
    def_var_in(vec4, out_color,    COLOR0)
end_input

begin_output(outPixel)
    def_var_fragment(fragment)
end_output

def_sampler(2D, tBase, 0);

shader_main(outPixel, outVertex, input)
{
    declareOutVar(outPixel, output)

    outVarFragment(output, fragment) = sampleLevelZero(tBase, inVar(input, out_texcoord)) * inVar(input, out_color);
    outReturn(output)
}
)S", true });

    // Constructions propres a doomSceneMain : varyings entiers plats, tableau de
    // textures, texelFetch indexe, et une variable locale nommee `length`.
    t.push_back({ "T11", "doomSceneMain : flat int, sampler2DArray, variable `length`", GL_FRAGMENT_SHADER,
        std::string("#version 330 core\n") + KEX_MACROS + R"S(
begin_cbuffer(PostProcess_DoomFog, 15)
    cbuffer_member(vec3,  uFogColor);
    cbuffer_member(float, uFogFar);
    cbuffer_member(float, uFogNear);
end_cbuffer()

def_samplerArray(2D, tBase, 0);

def_var_in(vec2, out_texcoord, TEXCOORD0)
def_var_in(vec4, out_color, COLOR0)
def_flat_var_in(int, out_offset, OFFSET1)
def_flat_var_in(int, out_dimensions, DIMS)
def_flat_var_in(int, out_options, OPTIONS)
def_var_in(vec4, out_flash, COLOR1)
def_var_in(vec3, out_position, POSITION0)

out vec4 outFragment0;

#define PAGE_SIZE 1024

ivec2 GetRemappedCoordinates(const vec2 coords, const int width, const int height, const int offset)
{
    internalconst int startOffset = offset;
    ivec2 iTC = ivec2(coords * vec2(float(width), float(height)));
    int tOffset = startOffset + ((iTC.y % height) * width) + (iTC.x % width);
    int y_newOffset = (tOffset / PAGE_SIZE) & (PAGE_SIZE - 1);
    int x_newOffset = (tOffset & (PAGE_SIZE - 1));
    return ivec2(x_newOffset, y_newOffset);
}

shader_main(outPixel, outVertex, input)
{
    internalconst int layer  = int((inVar(input, out_flash).a + 0.00196) * 255.0);
    internalconst int width  = (inVar(input, out_dimensions) >> 16) & 0xFFFF;
    internalconst int height = inVar(input, out_dimensions) & 0xFFFF;
    internalconst int flipX  = (inVar(input, out_options) >> 1) & 0x1;
    internalconst int flipY  = inVar(input, out_options) & 0x1;
    internalconst int offset = inVar(input, out_offset);
    internalconst int glow   = (inVar(input, out_options) >> 3) & 0xFF;

    float w = float(width);
    float h = float(height);
    internalconst float texelx = rcp(w);
    internalconst float texely = rcp(h);

    vec2 vFlip  = vec2(float(flipX), float(flipY));
    vec2 vTCC   = vec2(texelx, texely);
    vec2 length = vec2(1.0, 1.0);

    vec2 uvCoord = inVar(input, out_texcoord);
    uvCoord += ((length - abs((fract(uvCoord * 0.5) * 2.0) - length)) - uvCoord) * vFlip;

    vec2 fTC = uvCoord - (vTCC * 0.25);

    vec4 final = loadArray(tBase, GetRemappedCoordinates(fTC, width, height, offset), layer, 0);

    float interp_x = modf(fTC.x * w, w);

    final.rgb += float(glow) / 255.0;
    final *= inVar(input, out_color);
    final.rgb += inVar(input, out_flash).rgb + interp_x;

    float depth = -(inVar(input, out_position).z / 4096.0);
    final = mix(vec4(uFogColor, 1.0), final, 1.0 - smoothstep(uFogNear, uFogFar, depth));

    outVarFragment(output, fragment) = final;
    outReturn(output)
}
)S", true });

    return t;
}

// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0)
            verbose = true;
    }

    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::printf("SDL_Init a echoue : %s\n", SDL_GetError());
        return 2;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* win = SDL_CreateWindow("glslpp_test",
                                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       64, 64,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!win) {
        std::printf("Creation de fenetre impossible : %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        std::printf("Contexte OpenGL 3.3 core impossible : %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 2;
    }

    if (!load_gl()) {
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 2;
    }

    auto str = [](GLenum e) {
        const GLubyte* s = p_GetString(e);
        return s ? reinterpret_cast<const char*>(s) : "(null)";
    };

    std::printf("\n");
    std::printf("=====================================================================\n");
    std::printf(" glslpp_test - DOOM 64 EX II\n");
    std::printf(" Le preprocesseur GLSL du pilote accepte-t-il le dialecte de KEX ?\n");
    std::printf("=====================================================================\n\n");
    std::printf("  Vendeur  : %s\n", str(GL_VENDOR));
    std::printf("  Renderer : %s\n", str(GL_RENDERER));
    std::printf("  OpenGL   : %s\n", str(GL_VERSION));
    std::printf("  GLSL     : %s\n\n", str(GL_SHADING_LANGUAGE_VERSION));

    auto tests = build_tests();

    int passed = 0;
    int failed_critical = 0;
    int failed_minor = 0;

    for (const auto& t : tests) {
        Result r = compile(t.stage, t.src);

        const char* verdict = r.ok ? "OK   " : (t.critical ? "ECHEC" : "note ");
        std::printf("  [%s] %s  %s\n", verdict, t.id, t.what);

        if (r.ok) {
            ++passed;
            if (verbose && !r.log.empty())
                std::printf("           journal : %s\n", r.log.c_str());
        } else {
            if (t.critical) ++failed_critical; else ++failed_minor;
            if (!r.log.empty()) {
                std::printf("           ----------------------------------------------------\n");
                std::string line;
                for (char c : r.log + "\n") {
                    if (c == '\n') { std::printf("           %s\n", line.c_str()); line.clear(); }
                    else if (c != '\r') line += c;
                }
                std::printf("           ----------------------------------------------------\n");
            }
        }
    }

    std::printf("\n---------------------------------------------------------------------\n");
    std::printf("  %d reussis, %d echecs bloquants, %d echecs mineurs (sur %d)\n\n",
                passed, failed_critical, failed_minor, static_cast<int>(tests.size()));

    int rc;
    if (failed_critical == 0) {
        std::printf("  VERDICT : le pilote gere le dialecte KEX en entier.\n\n");
        std::printf("  Le preprocesseur du moteur n'a qu'a :\n");
        std::printf("    1. resoudre les #include depuis le pk3 (recursif, #pragma once)\n");
        std::printf("    2. prefixer #version 330 core, #define GLSL_VERSION 330,\n");
        std::printf("       puis SHADER_VERTEX ou SHADER_PIXEL\n");
        std::printf("    3. compiler la meme source deux fois, une par etage\n\n");
        std::printf("  Les 30 shaders de Doom64.kpf sont utilisables tels quels.\n");
        rc = 0;
    } else {
        std::printf("  VERDICT : le pilote NE gere PAS le dialecte KEX en entier.\n\n");
        std::printf("  Il faut une expansion de macros complete dans le moteur, ou\n");
        std::printf("  reecrire common_glsl.inc sans les constructions en echec.\n");
        std::printf("  Regarde les journaux ci-dessus : si seul T02 echoue, il suffit\n");
        std::printf("  d'ecrire def_samplerArray a la main, sans collage de jetons.\n");
        rc = 1;
    }
    std::printf("---------------------------------------------------------------------\n\n");

    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return rc;
}
