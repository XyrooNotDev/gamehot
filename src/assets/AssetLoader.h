#pragma once
#include <string>
#include "assets/AssetIO.h"
#include "core/MeshData.h"

namespace hotel {

// Loader model ringan (tanpa library pihak ketiga):
//  - glTF 2.0: .glb dan .gltf (buffer eksternal / base64), hierarki node, baseColorFactor,
//    baseColorTexture (PNG) -> dibakar ke warna vertex.
//  - Wavefront .obj (+ .mtl: Kd dan map_Kd PNG).
// Hasil: MeshData (posisi, normal, warna vertex) -> Renderer::createMesh.
class AssetLoader {
public:
    explicit AssetLoader(AssetIO* io) : io_(io) {}

    // Coba <base>.glb, <base>.gltf, <base>.obj berurutan. base mis. "models/bed_single".
    bool loadModel(const std::string& base, MeshData& out);

    bool loadGltfFile(const std::string& path, MeshData& out);
    bool loadObjFile(const std::string& path, MeshData& out);
    bool loadGltfMemory(const std::vector<uint8_t>& bytes, const std::string& baseDir, MeshData& out);

    // Skala+geser model: pusat XZ di 0, dasar di y=0.
    //  heightTarget > 0 : skala seragam agar tinggi = heightTarget (karakter)
    //  selain itu       : muat ke footprint w x d (meter) dengan margin
    static void fit(MeshData& m, float footprintW, float footprintD, float heightTarget = 0.0f);

private:
    AssetIO* io_;
};

} // namespace hotel
