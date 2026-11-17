#pragma once
#include <vector>
#include "core/MeshData.h"

namespace hotel {

enum class FurnType { Bed, Reception, Decor };

struct FurnitureDef {
    int id;                 // = indeks di katalog
    const char* name;       // tampil di toko (huruf besar, font bitmap)
    const char* modelName;  // opsional: assets/models/<modelName>.glb|.gltf|.obj
    FurnType type;
    int w, h;               // footprint tile pada rotasi 0 (w di sumbu X, h di sumbu Z)
    int price;
    int rate;               // pendapatan per tamu (khusus Bed)
    Color main, accent;
};

const std::vector<FurnitureDef>& furnitureCatalog();
const FurnitureDef* findFurniture(int id);

// Mesh prosedural (cadangan jika model aset tidak ada).
// Ruang lokal: pusat footprint di (0,0) pada XZ, dasar di y=0, depan = +Z.
MeshData buildProceduralFurniture(const FurnitureDef& def);

} // namespace hotel
