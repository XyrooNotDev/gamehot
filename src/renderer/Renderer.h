#pragma once
#include <cstdint>
#include <memory>
#include "core/Math.h"
#include "core/MeshData.h"
#include "ui/UIBatch.h"

namespace hotel {

struct MeshHandle {
    uint32_t vao = 0, vbo = 0, ibo = 0;
    int indexCount = 0;
    bool valid() const { return vao != 0; }
};

// Renderer OpenGL ES 3.0. Header sengaja bebas GL agar Game & logika lain
// bisa dikompilasi/dites tanpa GL.
class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init();
    void shutdown();
    void resize(int w, int h);

    MeshHandle createMesh(const MeshData& data);
    void destroyMesh(MeshHandle& mesh);

    void beginFrame(float r, float g, float b);
    // tint: pengali warna; tint.a < 1 -> digambar transparan (dipakai pratinjau penempatan furnitur).
    void drawMesh(const MeshHandle& mesh, const Mat4& model, const Mat4& viewProj, Color tint = Color{1, 1, 1, 1});
    void drawUI(const UIBatch& batch);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace hotel
