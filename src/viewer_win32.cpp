// Physarum 3D - native Win32/OpenGL viewer for the Visual Studio solution.
//
// Two render modes:
//   Volume  (default) - ray-marches the trail field as a glowing volume.
//   Points            - draws the agents as additive sprites (fallback / compare).
//
// No GLFW/GLAD: the .sln builds in Visual Studio with no external dependencies.
//
// Controls:
//   left-drag   orbit camera          mouse wheel  zoom
//   V / P       volume / points mode   Space        pause
//   [ / ]       exposure (volume)      - / =        ray steps (volume)
//   Up/Down     simulation speed       R            reset      Esc  quit

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <gl/GL.h>

#include "mat4.hpp"
#include "physarum.hpp"
#include "font_atlas.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER 0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_POINT_SPRITE
#define GL_POINT_SPRITE 0x8861
#endif
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif

using GLchar = char;
using GLsizeiptr = ptrdiff_t;

using PFNGLCREATESHADERPROC = GLuint(APIENTRY*)(GLenum type);
using PFNGLSHADERSOURCEPROC = void(APIENTRY*)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
using PFNGLCOMPILESHADERPROC = void(APIENTRY*)(GLuint shader);
using PFNGLGETSHADERIVPROC = void(APIENTRY*)(GLuint shader, GLenum pname, GLint* params);
using PFNGLGETSHADERINFOLOGPROC = void(APIENTRY*)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLCREATEPROGRAMPROC = GLuint(APIENTRY*)();
using PFNGLATTACHSHADERPROC = void(APIENTRY*)(GLuint program, GLuint shader);
using PFNGLLINKPROGRAMPROC = void(APIENTRY*)(GLuint program);
using PFNGLGETPROGRAMIVPROC = void(APIENTRY*)(GLuint program, GLenum pname, GLint* params);
using PFNGLGETPROGRAMINFOLOGPROC = void(APIENTRY*)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
using PFNGLDELETESHADERPROC = void(APIENTRY*)(GLuint shader);
using PFNGLGENVERTEXARRAYSPROC = void(APIENTRY*)(GLsizei n, GLuint* arrays);
using PFNGLBINDVERTEXARRAYPROC = void(APIENTRY*)(GLuint array);
using PFNGLGENBUFFERSPROC = void(APIENTRY*)(GLsizei n, GLuint* buffers);
using PFNGLBINDBUFFERPROC = void(APIENTRY*)(GLenum target, GLuint buffer);
using PFNGLBUFFERDATAPROC = void(APIENTRY*)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
using PFNGLENABLEVERTEXATTRIBARRAYPROC = void(APIENTRY*)(GLuint index);
using PFNGLVERTEXATTRIBPOINTERPROC = void(APIENTRY*)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
using PFNGLUSEPROGRAMPROC = void(APIENTRY*)(GLuint program);
using PFNGLGETUNIFORMLOCATIONPROC = GLint(APIENTRY*)(GLuint program, const GLchar* name);
using PFNGLUNIFORMMATRIX4FVPROC = void(APIENTRY*)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
using PFNGLUNIFORM3FPROC = void(APIENTRY*)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
using PFNGLUNIFORM1FPROC = void(APIENTRY*)(GLint location, GLfloat v0);
using PFNGLUNIFORM1IPROC = void(APIENTRY*)(GLint location, GLint v0);
using PFNGLACTIVETEXTUREPROC = void(APIENTRY*)(GLenum texture);
using PFNGLTEXIMAGE3DPROC = void(APIENTRY*)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels);
using PFNGLTEXSUBIMAGE3DPROC = void(APIENTRY*)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels);
using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int interval);

static PFNGLCREATESHADERPROC glCreateShader_ = nullptr;
static PFNGLSHADERSOURCEPROC glShaderSource_ = nullptr;
static PFNGLCOMPILESHADERPROC glCompileShader_ = nullptr;
static PFNGLGETSHADERIVPROC glGetShaderiv_ = nullptr;
static PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ = nullptr;
static PFNGLCREATEPROGRAMPROC glCreateProgram_ = nullptr;
static PFNGLATTACHSHADERPROC glAttachShader_ = nullptr;
static PFNGLLINKPROGRAMPROC glLinkProgram_ = nullptr;
static PFNGLGETPROGRAMIVPROC glGetProgramiv_ = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_ = nullptr;
static PFNGLDELETESHADERPROC glDeleteShader_ = nullptr;
static PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ = nullptr;
static PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ = nullptr;
static PFNGLGENBUFFERSPROC glGenBuffers_ = nullptr;
static PFNGLBINDBUFFERPROC glBindBuffer_ = nullptr;
static PFNGLBUFFERDATAPROC glBufferData_ = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ = nullptr;
static PFNGLUSEPROGRAMPROC glUseProgram_ = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation_ = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv_ = nullptr;
static PFNGLUNIFORM3FPROC glUniform3f_ = nullptr;
static PFNGLUNIFORM1FPROC glUniform1f_ = nullptr;
static PFNGLUNIFORM1IPROC glUniform1i_ = nullptr;
static PFNGLACTIVETEXTUREPROC glActiveTexture_ = nullptr;
static PFNGLTEXIMAGE3DPROC glTexImage3D_ = nullptr;
static PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D_ = nullptr;
static PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT_ = nullptr;

static void* getGlProcAddress(const char* name) {
    void* p = reinterpret_cast<void*>(wglGetProcAddress(name));
    const auto ip = reinterpret_cast<std::uintptr_t>(p);
    if (p == nullptr || ip == 1 || ip == 2 || ip == 3 || p == reinterpret_cast<void*>(-1)) {
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    }
    return p;
}

#define LOAD_GL(name) \
    do { \
        name##_ = reinterpret_cast<decltype(name##_)>(getGlProcAddress(#name)); \
        if (!name##_) { \
            MessageBoxA(nullptr, "Required OpenGL function is missing: " #name, "Physarum 3D", MB_ICONERROR | MB_OK); \
            return false; \
        } \
    } while (false)

static bool loadOpenGL() {
    LOAD_GL(glCreateShader);
    LOAD_GL(glShaderSource);
    LOAD_GL(glCompileShader);
    LOAD_GL(glGetShaderiv);
    LOAD_GL(glGetShaderInfoLog);
    LOAD_GL(glCreateProgram);
    LOAD_GL(glAttachShader);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glGetProgramInfoLog);
    LOAD_GL(glDeleteShader);
    LOAD_GL(glGenVertexArrays);
    LOAD_GL(glBindVertexArray);
    LOAD_GL(glGenBuffers);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glBufferData);
    LOAD_GL(glEnableVertexAttribArray);
    LOAD_GL(glVertexAttribPointer);
    LOAD_GL(glUseProgram);
    LOAD_GL(glGetUniformLocation);
    LOAD_GL(glUniformMatrix4fv);
    LOAD_GL(glUniform3f);
    LOAD_GL(glUniform1f);
    LOAD_GL(glUniform1i);
    LOAD_GL(glActiveTexture);
    LOAD_GL(glTexImage3D);
    LOAD_GL(glTexSubImage3D);

    wglSwapIntervalEXT_ = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(getGlProcAddress("wglSwapIntervalEXT"));
    if (wglSwapIntervalEXT_) wglSwapIntervalEXT_(1);
    return true;
}

// ---- Volume ray-march (fullscreen triangle) ----
static const char* VOL_VS = R"(#version 330 core
out vec2 vNdc;
void main(){
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                  (gl_VertexID == 2) ? 3.0 : -1.0);
    vNdc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

static const char* VOL_FS = R"(#version 330 core
in vec2 vNdc;
out vec4 frag;
uniform sampler3D uVolume;
uniform vec3 uEye, uForward, uRight, uUp;
uniform float uTanHalf, uAspect, uGrid;
uniform float uDensityScale, uExposure, uAbsorption;
uniform int uSteps;

void main(){
    vec3 rd = normalize(uForward
        + vNdc.x * uTanHalf * uAspect * uRight
        + vNdc.y * uTanHalf * uUp);
    vec3 ro = uEye;
    vec3 bg = vec3(0.015, 0.015, 0.03);

    vec3 invD = 1.0 / rd;
    vec3 t0 = (vec3(0.0) - ro) * invD;
    vec3 t1 = (vec3(uGrid) - ro) * invD;
    vec3 tsm = min(t0, t1);
    vec3 tbg = max(t0, t1);
    float tNear = max(max(tsm.x, tsm.y), tsm.z);
    float tFar  = min(min(tbg.x, tbg.y), tbg.z);
    if (tFar <= max(tNear, 0.0)) { frag = vec4(bg, 1.0); return; }
    tNear = max(tNear, 0.0);

    float dt = (tFar - tNear) / float(uSteps);
    vec3 col = vec3(0.0);
    float alpha = 0.0;
    float t = tNear + dt * 0.5;
    for (int i = 0; i < 512; ++i) {
        if (i >= uSteps) break;
        vec3 uvw = (ro + rd * t) / uGrid;
        float raw = texture(uVolume, uvw).r;
        float d = clamp(raw * uDensityScale, 0.0, 1.0);
        if (d > 0.002) {
            vec3 c1 = vec3(0.00, 0.18, 0.30);   // faint teal
            vec3 c2 = vec3(0.05, 0.70, 0.78);   // cyan
            vec3 c3 = vec3(0.90, 1.00, 0.92);   // hot white-green
            vec3 tc = mix(c1, c2, smoothstep(0.0, 0.45, d));
            tc = mix(tc, c3, smoothstep(0.45, 1.0, d));
            float a = 1.0 - exp(-d * uAbsorption * dt);
            vec3 emit = tc * d * uExposure;
            col += (1.0 - alpha) * emit * a;
            alpha += (1.0 - alpha) * a;
            if (alpha > 0.992) break;
        }
        t += dt;
    }
    col += (1.0 - alpha) * bg;
    frag = vec4(col, 1.0);
}
)";

// ---- Agent points (additive) ----
static const char* POINT_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aTrail;
uniform mat4 uViewProj;
uniform float uPointScale;
uniform float uTrailScale;
out float vT;
void main(){
    vec4 clip = uViewProj * vec4(aPos, 1.0);
    gl_Position = clip;
    gl_PointSize = clamp(uPointScale / max(clip.w, 0.001), 1.0, 32.0);
    vT = clamp(aTrail * uTrailScale, 0.0, 1.0);
}
)";

static const char* POINT_FS = R"(#version 330 core
in float vT;
out vec4 frag;
uniform float uBrightness;
void main(){
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0) discard;
    float a = exp(-r2 * 3.0);
    vec3 dim = vec3(0.10, 0.35, 0.55);
    vec3 hot = vec3(0.75, 0.95, 1.00);
    vec3 col = mix(dim, hot, vT);
    frag = vec4(col * uBrightness * a, a);
}
)";

static const char* LINE_VS = R"(#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uViewProj;
void main(){ gl_Position = uViewProj * vec4(aPos, 1.0); }
)";

static const char* LINE_FS = R"(#version 330 core
out vec4 frag;
uniform vec3 uColor;
void main(){ frag = vec4(uColor, 1.0); }
)";

// ---- HUD overlay (screen-space quads: panel + bitmap text) ----
static const char* HUD_VS = R"(#version 330 core
layout(location=0) in vec2 aPos;   // already in NDC
layout(location=1) in vec2 aUV;
out vec2 vUV;
void main(){ vUV = aUV; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

static const char* HUD_FS = R"(#version 330 core
in vec2 vUV;
out vec4 frag;
uniform sampler2D uFont;
uniform vec3 uColor;
uniform float uAlpha;
uniform int uTextured;
void main(){
    if (uTextured == 1) {
        float cov = texture(uFont, vUV).r;
        frag = vec4(uColor, uAlpha * cov);
    } else {
        frag = vec4(uColor, uAlpha);
    }
}
)";

static void debugLog(const char* text) {
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
}

static GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader_(type);
    glShaderSource_(shader, 1, &src, nullptr);
    glCompileShader_(shader);
    GLint ok = 0;
    glGetShaderiv_(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog_(shader, sizeof(log), nullptr, log);
        debugLog(log);
        MessageBoxA(nullptr, log, "Shader compile error", MB_ICONERROR | MB_OK);
    }
    return shader;
}

static GLuint linkProgram(const char* vs, const char* fs) {
    GLuint program = glCreateProgram_();
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    glAttachShader_(program, v);
    glAttachShader_(program, f);
    glLinkProgram_(program);
    glDeleteShader_(v);
    glDeleteShader_(f);
    GLint ok = 0;
    glGetProgramiv_(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetProgramInfoLog_(program, sizeof(log), nullptr, log);
        debugLog(log);
        MessageBoxA(nullptr, log, "Shader link error", MB_ICONERROR | MB_OK);
    }
    return program;
}

static std::unique_ptr<Physarum> makeSim() {
    PhysarumParams p;
    p.grid = 128;
    p.agents = 1'200'000;
    auto sim = std::make_unique<Physarum>(p);
    sim->setThreads(static_cast<int>(std::thread::hardware_concurrency()));
    return sim;
}

static void buildBoxLines(float g, std::vector<float>& out) {
    Vec3 a{0, 0, 0}, b{g, g, g};
    Vec3 c[8] = {{a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,b.y,a.z},{a.x,b.y,a.z},
                 {a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{a.x,b.y,b.z}};
    int e[24] = {0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7};
    out.clear();
    for (int i : e) { out.push_back(c[i].x); out.push_back(c[i].y); out.push_back(c[i].z); }
}

struct ViewerState {
    HWND hwnd = nullptr;
    HDC hdc = nullptr;
    HGLRC glrc = nullptr;

    OrbitCamera camera;
    bool dragging = false;
    bool paused = false;
    bool volumeMode = true;
    double lastX = 0.0;
    double lastY = 0.0;
    int substeps = 1;
    int raySteps = 96;
    float exposure = 1.7f;

    GLuint volProgram = 0, pointProgram = 0, lineProgram = 0, hudProgram = 0;
    GLuint volVao = 0, pointVao = 0, pointVbo = 0, lineVao = 0, lineVbo = 0;
    GLuint hudVao = 0, hudVbo = 0, fontTex = 0;
    GLuint volTex = 0;
    int gridN = 0;
    int vpW = 1280, vpH = 800;
    double fps = 0.0;

    std::unique_ptr<Physarum> sim;
    std::vector<float> boxLines;
    std::vector<float> interleaved;

    int frames = 0;
    std::chrono::steady_clock::time_point lastTitleUpdate = std::chrono::steady_clock::now();
};

static ViewerState* g_state = nullptr;

static void resetSimulation(ViewerState& s) {
    s.sim = makeSim();
}

static bool createOpenGLContext(ViewerState& s) {
    s.hdc = GetDC(s.hwnd);
    if (!s.hdc) return false;

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(s.hdc, &pfd);
    if (format == 0 || !SetPixelFormat(s.hdc, format, &pfd)) return false;

    s.glrc = wglCreateContext(s.hdc);
    if (!s.glrc || !wglMakeCurrent(s.hdc, s.glrc)) return false;

    return loadOpenGL();
}

static bool initRenderer(ViewerState& s) {
    s.sim = makeSim();
    s.gridN = s.sim->gridSize();
    float g = static_cast<float>(s.gridN);
    s.camera.target = {g * 0.5f, g * 0.5f, g * 0.5f};
    s.camera.distance = g * 1.7f;

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE); // compat-profile: required for gl_PointCoord

    s.volProgram = linkProgram(VOL_VS, VOL_FS);
    s.pointProgram = linkProgram(POINT_VS, POINT_FS);
    s.lineProgram = linkProgram(LINE_VS, LINE_FS);
    s.hudProgram = linkProgram(HUD_VS, HUD_FS);

    // font atlas -> 2D texture (single channel coverage)
    glGenTextures(1, &s.fontTex);
    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.fontTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, FONT_W, FONT_H, 0, GL_RED, GL_UNSIGNED_BYTE, FONT_PIX);

    glGenVertexArrays_(1, &s.hudVao);
    glGenBuffers_(1, &s.hudVbo);
    glBindVertexArray_(s.hudVao);
    glBindBuffer_(GL_ARRAY_BUFFER, s.hudVbo);
    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray_(1);
    glVertexAttribPointer_(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    // empty VAO for the fullscreen triangle
    glGenVertexArrays_(1, &s.volVao);

    // 3D texture for the trail field
    glGenTextures(1, &s.volTex);
    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, s.volTex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexImage3D_(GL_TEXTURE_3D, 0, GL_R32F, s.gridN, s.gridN, s.gridN, 0,
                  GL_RED, GL_FLOAT, s.sim->field().data().data());

    glGenVertexArrays_(1, &s.pointVao);
    glGenBuffers_(1, &s.pointVbo);
    glBindVertexArray_(s.pointVao);
    glBindBuffer_(GL_ARRAY_BUFFER, s.pointVbo);
    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray_(1);
    glVertexAttribPointer_(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

    glGenVertexArrays_(1, &s.lineVao);
    glGenBuffers_(1, &s.lineVbo);
    buildBoxLines(g, s.boxLines);
    glBindVertexArray_(s.lineVao);
    glBindBuffer_(GL_ARRAY_BUFFER, s.lineVbo);
    glBufferData_(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s.boxLines.size() * sizeof(float)), s.boxLines.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));

    RECT rc{};
    GetClientRect(s.hwnd, &rc);
    const int width = std::max(1L, rc.right - rc.left);
    const int height = std::max(1L, rc.bottom - rc.top);
    s.vpW = width;
    s.vpH = height;
    s.camera.aspect = static_cast<float>(width) / static_cast<float>(height);
    glViewport(0, 0, width, height);
    return true;
}

static void drawCube(ViewerState& s, const Mat4& vp, float r, float g, float b) {
    glDisable(GL_BLEND);
    glUseProgram_(s.lineProgram);
    glUniformMatrix4fv_(glGetUniformLocation_(s.lineProgram, "uViewProj"), 1, GL_FALSE, vp.data());
    glUniform3f_(glGetUniformLocation_(s.lineProgram, "uColor"), r, g, b);
    glBindVertexArray_(s.lineVao);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s.boxLines.size() / 3));
}

static void renderVolume(ViewerState& s) {
    const int G = s.gridN;
    const auto& d = s.sim->field().data();

    // auto-exposure: normalise by the current field maximum
    float mx = 1e-3f;
    for (float v : d) mx = std::max(mx, v);
    const float densityScale = 1.0f / (0.32f * mx);

    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, s.volTex);
    glTexSubImage3D_(GL_TEXTURE_3D, 0, 0, 0, 0, G, G, G, GL_RED, GL_FLOAT, d.data());

    const Vec3 eye = s.camera.eye();
    const Vec3 fwd = (s.camera.target - eye).normalized();
    const Vec3 right = fwd.cross(Vec3{0, 1, 0}).normalized();
    const Vec3 up = right.cross(fwd);
    const float tanHalf = std::tan(s.camera.fov * 0.5f);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram_(s.volProgram);
    glUniform1i_(glGetUniformLocation_(s.volProgram, "uVolume"), 0);
    glUniform3f_(glGetUniformLocation_(s.volProgram, "uEye"), eye.x, eye.y, eye.z);
    glUniform3f_(glGetUniformLocation_(s.volProgram, "uForward"), fwd.x, fwd.y, fwd.z);
    glUniform3f_(glGetUniformLocation_(s.volProgram, "uRight"), right.x, right.y, right.z);
    glUniform3f_(glGetUniformLocation_(s.volProgram, "uUp"), up.x, up.y, up.z);
    glUniform1f_(glGetUniformLocation_(s.volProgram, "uTanHalf"), tanHalf);
    glUniform1f_(glGetUniformLocation_(s.volProgram, "uAspect"), s.camera.aspect);
    glUniform1f_(glGetUniformLocation_(s.volProgram, "uGrid"), static_cast<float>(G));
    glUniform1f_(glGetUniformLocation_(s.volProgram, "uDensityScale"), densityScale);
    glUniform1f_(glGetUniformLocation_(s.volProgram, "uExposure"), s.exposure);
    glUniform1f_(glGetUniformLocation_(s.volProgram, "uAbsorption"), 2.5f);
    glUniform1i_(glGetUniformLocation_(s.volProgram, "uSteps"), s.raySteps);
    glBindVertexArray_(s.volVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    drawCube(s, s.camera.viewProj(), 0.10f, 0.13f, 0.20f);
}

static void renderPoints(ViewerState& s) {
    const auto& pos = s.sim->positions();
    const auto& trail = s.sim->agentTrail();
    const size_t n = pos.size();
    s.interleaved.resize(n * 4);
    for (size_t i = 0; i < n; ++i) {
        s.interleaved[i * 4 + 0] = pos[i].x;
        s.interleaved[i * 4 + 1] = pos[i].y;
        s.interleaved[i * 4 + 2] = pos[i].z;
        s.interleaved[i * 4 + 3] = trail[i];
    }
    Mat4 vp = s.camera.viewProj();
    glDisable(GL_DEPTH_TEST);
    drawCube(s, vp, 0.12f, 0.16f, 0.24f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindBuffer_(GL_ARRAY_BUFFER, s.pointVbo);
    glBufferData_(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(s.interleaved.size() * sizeof(float)), s.interleaved.data(), GL_STREAM_DRAW);
    glUseProgram_(s.pointProgram);
    glUniformMatrix4fv_(glGetUniformLocation_(s.pointProgram, "uViewProj"), 1, GL_FALSE, vp.data());
    glUniform1f_(glGetUniformLocation_(s.pointProgram, "uPointScale"), 520.0f);
    glUniform1f_(glGetUniformLocation_(s.pointProgram, "uTrailScale"), 1.0f / 250.0f);
    glUniform1f_(glGetUniformLocation_(s.pointProgram, "uBrightness"), 0.5f);
    glBindVertexArray_(s.pointVao);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(n));
}

static void renderHUD(ViewerState& s) {
    // Build the overlay text lines.
    char buf[128];
    std::vector<std::string> lines;
    lines.emplace_back("Physarum 3D");
    std::snprintf(buf, sizeof(buf), "fps %.0f", s.fps);
    lines.emplace_back(buf);
    if (s.volumeMode)
        std::snprintf(buf, sizeof(buf), "volume  %d^3  steps %d  exp %.1f", s.gridN, s.raySteps, s.exposure);
    else
        std::snprintf(buf, sizeof(buf), "points  %.1fM agents", s.sim->agentCount() / 1.0e6);
    lines.emplace_back(buf);
    std::snprintf(buf, sizeof(buf), "speed x%d%s", s.substeps, s.paused ? "   [PAUSED]" : "");
    lines.emplace_back(buf);
    lines.emplace_back("");
    lines.emplace_back("V / P    volume / points");
    lines.emplace_back("[ / ]    exposure");
    lines.emplace_back("- / =    ray steps");
    lines.emplace_back("up / dn  sim speed");
    lines.emplace_back("space    pause");
    lines.emplace_back("R        reset");
    lines.emplace_back("drag / wheel  orbit / zoom");
    lines.emplace_back("esc      quit");

    const float sc = 1.55f;                 // text scale
    const float gw = FONT_CW * sc;          // glyph cell on screen
    const float gh = FONT_CH * sc;
    const float pad = 10.0f, marginX = 12.0f, marginY = 10.0f;

    size_t maxChars = 0;
    for (auto& ln : lines) maxChars = std::max(maxChars, ln.size());
    const float panelW = marginX + maxChars * gw + pad;
    const float panelH = marginY + lines.size() * gh + pad;

    auto ndcX = [&](float px) { return px / s.vpW * 2.0f - 1.0f; };
    auto ndcY = [&](float py) { return 1.0f - py / s.vpH * 2.0f; };

    std::vector<float> v;
    v.reserve(2048);
    auto quad = [&](float x0, float y0, float x1, float y1,
                    float u0, float w0, float u1, float w1) {
        float ax = ndcX(x0), ay = ndcY(y0), bx = ndcX(x1), by = ndcY(y1);
        float r[6][4] = {{ax,ay,u0,w0},{bx,ay,u1,w0},{bx,by,u1,w1},
                         {ax,ay,u0,w0},{bx,by,u1,w1},{ax,by,u0,w1}};
        for (auto& q : r) { v.push_back(q[0]); v.push_back(q[1]); v.push_back(q[2]); v.push_back(q[3]); }
    };

    // [0,6) = background panel (uv unused)
    quad(4.0f, 4.0f, 4.0f + panelW, 4.0f + panelH, 0, 0, 0, 0);

    // text quads
    for (size_t li = 0; li < lines.size(); ++li) {
        const std::string& ln = lines[li];
        float y0 = 4.0f + marginY + li * gh;
        for (size_t ci = 0; ci < ln.size(); ++ci) {
            int code = static_cast<unsigned char>(ln[ci]);
            if (code <= 32 || code >= FONT_FIRST + FONT_COUNT) continue; // skip space/non-printable
            int idx = code - FONT_FIRST;
            float x0 = 4.0f + marginX + ci * gw;
            float u0 = (idx * FONT_CW) / static_cast<float>(FONT_W);
            float u1 = (idx * FONT_CW + FONT_CW) / static_cast<float>(FONT_W);
            quad(x0, y0, x0 + gw, y0 + gh, u0, 0.0f, u1, 1.0f);
        }
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s.fontTex);

    glBindBuffer_(GL_ARRAY_BUFFER, s.hudVbo);
    glBufferData_(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STREAM_DRAW);
    glUseProgram_(s.hudProgram);
    glUniform1i_(glGetUniformLocation_(s.hudProgram, "uFont"), 0);
    glBindVertexArray_(s.hudVao);

    // panel
    glUniform1i_(glGetUniformLocation_(s.hudProgram, "uTextured"), 0);
    glUniform3f_(glGetUniformLocation_(s.hudProgram, "uColor"), 0.02f, 0.03f, 0.06f);
    glUniform1f_(glGetUniformLocation_(s.hudProgram, "uAlpha"), 0.55f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // text
    glUniform1i_(glGetUniformLocation_(s.hudProgram, "uTextured"), 1);
    glUniform3f_(glGetUniformLocation_(s.hudProgram, "uColor"), 0.86f, 0.95f, 1.0f);
    glUniform1f_(glGetUniformLocation_(s.hudProgram, "uAlpha"), 1.0f);
    glDrawArrays(GL_TRIANGLES, 6, static_cast<GLsizei>(v.size() / 4 - 6));
}

static void renderFrame(ViewerState& s) {
    if (!s.paused) {
        for (int i = 0; i < s.substeps; ++i) s.sim->step();
    }

    glClearColor(0.015f, 0.015f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (s.volumeMode) renderVolume(s);
    else renderPoints(s);

    renderHUD(s);

    SwapBuffers(s.hdc);

    ++s.frames;
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - s.lastTitleUpdate).count();
    if (dt >= 0.5) {
        const double fps = static_cast<double>(s.frames) / dt;
        s.fps = fps;
        s.frames = 0;
        s.lastTitleUpdate = now;
        char title[224];
        if (s.volumeMode)
            std::snprintf(title, sizeof(title),
                "Physarum 3D  |  volume  |  %d^3  |  %.0f fps  |  steps %d  exp %.1f  x%d%s",
                s.gridN, fps, s.raySteps, s.exposure, s.substeps, s.paused ? "  [PAUSED]" : "");
        else
            std::snprintf(title, sizeof(title),
                "Physarum 3D  |  points  |  %zu agents  |  %.0f fps  |  x%d%s",
                s.sim->agentCount(), fps, s.substeps, s.paused ? "  [PAUSED]" : "");
        SetWindowTextA(s.hwnd, title);
    }
}

static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ViewerState* s = g_state;
    switch (msg) {
    case WM_SIZE:
        if (s) {
            int width = std::max(1, static_cast<int>(LOWORD(lParam)));
            int height = std::max(1, static_cast<int>(HIWORD(lParam)));
            s->vpW = width;
            s->vpH = height;
            s->camera.aspect = static_cast<float>(width) / static_cast<float>(height);
            if (s->glrc) glViewport(0, 0, width, height);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (s) {
            s->dragging = true;
            s->lastX = static_cast<double>(GET_X_LPARAM(lParam));
            s->lastY = static_cast<double>(GET_Y_LPARAM(lParam));
            SetCapture(hwnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (s) { s->dragging = false; ReleaseCapture(); }
        return 0;

    case WM_MOUSEMOVE:
        if (s) {
            const double x = static_cast<double>(GET_X_LPARAM(lParam));
            const double y = static_cast<double>(GET_Y_LPARAM(lParam));
            if (s->dragging) s->camera.rotate(static_cast<float>(x - s->lastX), static_cast<float>(y - s->lastY));
            s->lastX = x;
            s->lastY = y;
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (s) {
            const float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
            s->camera.zoom(delta);
        }
        return 0;

    case WM_KEYDOWN:
        if (s) {
            switch (wParam) {
            case VK_SPACE: s->paused = !s->paused; break;
            case VK_UP:    s->substeps = std::min(s->substeps + 1, 8); break;
            case VK_DOWN:  s->substeps = std::max(s->substeps - 1, 1); break;
            case 'V':      s->volumeMode = true; break;
            case 'P':      s->volumeMode = false; break;
            case 'R':      resetSimulation(*s); break;
            case VK_OEM_4: s->exposure = std::max(0.2f, s->exposure - 0.2f); break;   // [
            case VK_OEM_6: s->exposure = std::min(8.0f, s->exposure + 0.2f); break;   // ]
            case VK_OEM_MINUS: s->raySteps = std::max(24, s->raySteps - 16); break;   // -
            case VK_OEM_PLUS:  s->raySteps = std::min(512, s->raySteps + 16); break;  // =
            case VK_ESCAPE: DestroyWindow(hwnd); break;
            }
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    SetProcessDPIAware();

    ViewerState state;
    g_state = &state;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "PhysarumWindow";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(nullptr, "RegisterClassEx failed", "Physarum 3D", MB_ICONERROR | MB_OK);
        return 1;
    }

    RECT rect{0, 0, 1280, 800};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    state.hwnd = CreateWindowExA(
        0, wc.lpszClassName, "Physarum 3D",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!state.hwnd) {
        MessageBoxA(nullptr, "CreateWindowEx failed", "Physarum 3D", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!createOpenGLContext(state)) {
        MessageBoxA(nullptr, "Could not create an OpenGL context", "Physarum 3D", MB_ICONERROR | MB_OK);
        return 1;
    }

    if (!initRenderer(state)) {
        MessageBoxA(nullptr, "Renderer initialization failed", "Physarum 3D", MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(state.hwnd, nCmdShow);
    UpdateWindow(state.hwnd);

    MSG msg{};
    bool running = true;
    while (running) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (running) renderFrame(state);
    }

    if (state.glrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(state.glrc);
    }
    if (state.hwnd && state.hdc) ReleaseDC(state.hwnd, state.hdc);
    return 0;
}
