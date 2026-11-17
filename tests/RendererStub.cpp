// Renderer palsu (tanpa GL) agar Game bisa disimulasikan penuh di desktop.
#include "renderer/Renderer.h"

namespace hotel {
struct Renderer::Impl { int meshes = 0, draws = 0, uiVerts = 0; };
Renderer::Renderer() : impl_(new Impl) {}
Renderer::~Renderer() = default;
bool Renderer::init() { return true; }
void Renderer::shutdown() {}
void Renderer::resize(int, int) {}
MeshHandle Renderer::createMesh(const MeshData& d) { MeshHandle m; m.vao = ++impl_->meshes; m.indexCount = static_cast<int>(d.indices.size()); return m; }
void Renderer::destroyMesh(MeshHandle& m) { m = MeshHandle{}; }
void Renderer::beginFrame(float, float, float) {}
void Renderer::drawMesh(const MeshHandle&, const Mat4&, const Mat4&, Color) { ++impl_->draws; }
void Renderer::drawUI(const UIBatch& b) { impl_->uiVerts += static_cast<int>(b.vertices().size()); }
}
