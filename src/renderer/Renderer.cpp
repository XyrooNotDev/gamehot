#include "renderer/Renderer.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cstddef>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "HotelTycoon", __VA_ARGS__)

namespace hotel {

namespace {

const char* kMeshVS = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vNormal = mat3(uModel) * aNormal;   // aman selama model hanya translasi/rotasi
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
})";

const char* kMeshFS = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
in vec3 vColor;
uniform vec3 uLightDir;
uniform vec4 uTint;
out vec4 oColor;
void main() {
    float d = max(dot(normalize(vNormal), normalize(uLightDir)), 0.0);
    oColor = vec4(vColor * (0.45 + 0.55 * d) * uTint.rgb, uTint.a);
})";

const char* kUIVS = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform vec2 uScreen;
out vec4 vColor;
void main() {
    vec2 ndc = vec2(aPos.x / uScreen.x * 2.0 - 1.0, 1.0 - aPos.y / uScreen.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vColor = aColor;
})";

const char* kUIFS = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 oColor;
void main() { oColor = vColor; })";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOGE("Shader error: %s", log);
    }
    return s;
}

GLuint link(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        LOGE("Link error: %s", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

} // namespace

struct Renderer::Impl {
    GLuint meshProg = 0, uiProg = 0;
    GLint uMVP = -1, uModel = -1, uLight = -1, uTint = -1, uScreen = -1;
    GLuint uiVao = 0, uiVbo = 0;
    int w = 1, h = 1;
};

Renderer::Renderer() : impl_(new Impl) {}
Renderer::~Renderer() = default;

bool Renderer::init() {
    Impl& s = *impl_;
    s.meshProg = link(kMeshVS, kMeshFS);
    s.uMVP = glGetUniformLocation(s.meshProg, "uMVP");
    s.uModel = glGetUniformLocation(s.meshProg, "uModel");
    s.uLight = glGetUniformLocation(s.meshProg, "uLightDir");
    s.uTint = glGetUniformLocation(s.meshProg, "uTint");

    s.uiProg = link(kUIVS, kUIFS);
    s.uScreen = glGetUniformLocation(s.uiProg, "uScreen");

    glGenVertexArrays(1, &s.uiVao);
    glGenBuffers(1, &s.uiVbo);
    glBindVertexArray(s.uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, s.uiVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)offsetof(UIVertex, r));
    glBindVertexArray(0);
    return s.meshProg != 0 && s.uiProg != 0;
}

void Renderer::shutdown() {
    Impl& s = *impl_;
    if (s.meshProg) glDeleteProgram(s.meshProg);
    if (s.uiProg) glDeleteProgram(s.uiProg);
    if (s.uiVbo) glDeleteBuffers(1, &s.uiVbo);
    if (s.uiVao) glDeleteVertexArrays(1, &s.uiVao);
    s = Impl{};
}

void Renderer::resize(int w, int h) { impl_->w = w; impl_->h = h; }

MeshHandle Renderer::createMesh(const MeshData& d) {
    MeshHandle m;
    m.indexCount = static_cast<int>(d.indices.size());
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ibo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, d.vertices.size() * sizeof(Vertex), d.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, d.indices.size() * sizeof(uint32_t), d.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, px));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glBindVertexArray(0);
    return m;
}

void Renderer::destroyMesh(MeshHandle& m) {
    if (!m.valid()) return;
    glDeleteBuffers(1, &m.vbo);
    glDeleteBuffers(1, &m.ibo);
    glDeleteVertexArrays(1, &m.vao);
    m = MeshHandle{};
}

void Renderer::beginFrame(float r, float g, float b) {
    glViewport(0, 0, impl_->w, impl_->h);
    glClearColor(r, g, b, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawMesh(const MeshHandle& mesh, const Mat4& model, const Mat4& viewProj, Color tint) {
    if (!mesh.valid()) return;
    Impl& s = *impl_;
    Mat4 mvp = viewProj * model;
    glUseProgram(s.meshProg);
    glUniformMatrix4fv(s.uMVP, 1, GL_FALSE, mvp.m);
    glUniformMatrix4fv(s.uModel, 1, GL_FALSE, model.m);
    glUniform3f(s.uLight, 0.35f, 0.85f, 0.40f);
    glUniform4f(s.uTint, tint.r, tint.g, tint.b, tint.a);
    bool blend = tint.a < 0.999f;
    if (blend) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }
    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    if (blend) glDisable(GL_BLEND);
}

void Renderer::drawUI(const UIBatch& batch) {
    if (batch.vertices().empty()) return;
    Impl& s = *impl_;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(s.uiProg);
    glUniform2f(s.uScreen, static_cast<float>(s.w), static_cast<float>(s.h));
    glBindVertexArray(s.uiVao);
    glBindBuffer(GL_ARRAY_BUFFER, s.uiVbo);
    glBufferData(GL_ARRAY_BUFFER, batch.vertices().size() * sizeof(UIVertex),
                 batch.vertices().data(), GL_STREAM_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch.vertices().size()));
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

} // namespace hotel
