#include "furniture/FurnitureCatalog.h"

namespace hotel {

const std::vector<FurnitureDef>& furnitureCatalog() {
    static const std::vector<FurnitureDef> kCatalog = {
        {0, "KASUR",       "bed_single", FurnType::Bed,       1, 2, 120, 45, {0.92f,0.92f,0.95f,1}, {0.30f,0.45f,0.80f,1}},
        {1, "KASUR BESAR", "bed_double", FurnType::Bed,       2, 2, 260, 100,{0.95f,0.90f,0.90f,1}, {0.80f,0.30f,0.35f,1}},
        {2, "RESEPSI",     "reception",  FurnType::Reception, 2, 1, 150, 0,  {0.55f,0.36f,0.22f,1}, {0.85f,0.75f,0.30f,1}},
        {3, "KURSI",       "chair",      FurnType::Decor,     1, 1, 25,  0,  {0.60f,0.40f,0.25f,1}, {0.35f,0.55f,0.35f,1}},
        {4, "MEJA",        "table",      FurnType::Decor,     1, 1, 40,  0,  {0.65f,0.45f,0.28f,1}, {0.50f,0.35f,0.20f,1}},
        {5, "TANAMAN",     "plant",      FurnType::Decor,     1, 1, 30,  0,  {0.55f,0.35f,0.22f,1}, {0.25f,0.65f,0.30f,1}},
        {6, "SOFA",        "sofa",       FurnType::Decor,     2, 1, 80,  0,  {0.30f,0.38f,0.60f,1}, {0.22f,0.28f,0.48f,1}},
    };
    return kCatalog;
}

const FurnitureDef* findFurniture(int id) {
    const auto& c = furnitureCatalog();
    return (id >= 0 && id < static_cast<int>(c.size())) ? &c[id] : nullptr;
}

MeshData buildProceduralFurniture(const FurnitureDef& d) {
    MeshData m;
    const float hw = d.w * 0.5f, hd = d.h * 0.5f;   // setengah lebar / kedalaman
    const Color white{0.96f, 0.96f, 0.96f, 1};
    const Color dark{0.20f, 0.14f, 0.10f, 1};

    switch (d.id) {
    case 0: case 1: {  // kasur: kepala di -Z
        float p = 0.06f;
        m.addBox({-hw + p, 0.0f, -hd + p}, {hw - p, 0.35f, hd - p}, d.main);                // rangka
        m.addBox({-hw + p + 0.03f, 0.35f, -hd + p + 0.03f}, {hw - p - 0.03f, 0.55f, hd - p - 0.03f}, white);  // kasur
        m.addBox({-hw + p, 0.55f, -hd + p + 0.55f}, {hw - p, 0.60f, hd - p}, d.accent);      // selimut
        float pw = (d.w == 1) ? 0.5f : 0.7f;
        for (int i = 0; i < d.w; ++i) {                                                    // bantal
            float cx = -hw + (i + 0.5f) * (d.w * 1.0f / d.w);
            m.addBox({cx - pw * 0.5f, 0.55f, -hd + p + 0.08f}, {cx + pw * 0.5f, 0.68f, -hd + p + 0.45f}, white);
        }
        m.addBox({-hw + p, 0.0f, -hd}, {hw - p, 0.95f, -hd + p}, dark);                      // papan kepala
        break;
    }
    case 2: {  // meja resepsionis
        m.addBox({-hw + 0.05f, 0.0f, -0.40f}, {hw - 0.05f, 0.95f, 0.30f}, d.main);
        m.addBox({-hw, 0.95f, -0.45f}, {hw, 1.03f, 0.40f}, dark);
        m.addBox({-0.12f, 1.03f, 0.05f}, {0.12f, 1.10f, 0.25f}, d.accent);                 // lonceng
        m.addBox({-0.04f, 1.10f, 0.12f}, {0.04f, 1.16f, 0.18f}, white);
        m.addBox({hw - 0.55f, 1.03f, -0.30f}, {hw - 0.15f, 1.35f, -0.22f}, {0.1f, 0.1f, 0.12f, 1});  // layar
        break;
    }
    case 3: {  // kursi, sandaran di -Z
        m.addBox({-0.25f, 0.40f, -0.25f}, {0.25f, 0.48f, 0.25f}, d.main);
        m.addBox({-0.25f, 0.48f, -0.25f}, {0.25f, 1.00f, -0.18f}, d.main);
        for (float sx : {-0.22f, 0.22f}) for (float sz : {-0.22f, 0.22f})
            m.addBox({sx - 0.03f, 0.0f, sz - 0.03f}, {sx + 0.03f, 0.40f, sz + 0.03f}, dark);
        break;
    }
    case 4: {  // meja
        m.addBox({-0.38f, 0.68f, -0.38f}, {0.38f, 0.76f, 0.38f}, d.main);
        m.addBox({-0.06f, 0.0f, -0.06f}, {0.06f, 0.68f, 0.06f}, d.accent);
        m.addBox({-0.22f, 0.0f, -0.22f}, {0.22f, 0.04f, 0.22f}, d.accent);
        break;
    }
    case 5: {  // tanaman
        m.addBox({-0.20f, 0.0f, -0.20f}, {0.20f, 0.35f, 0.20f}, d.main);
        m.addBox({-0.22f, 0.35f, -0.22f}, {0.22f, 0.65f, 0.22f}, d.accent);
        m.addBox({-0.15f, 0.65f, -0.15f}, {0.15f, 0.90f, 0.15f}, {0.30f, 0.75f, 0.35f, 1});
        m.addBox({-0.08f, 0.90f, -0.08f}, {0.08f, 1.05f, 0.08f}, {0.35f, 0.80f, 0.40f, 1});
        break;
    }
    case 6: {  // sofa, sandaran di -Z
        m.addBox({-hw + 0.05f, 0.0f, -0.40f}, {hw - 0.05f, 0.40f, 0.40f}, d.main);
        m.addBox({-hw + 0.05f, 0.40f, -0.40f}, {hw - 0.05f, 0.90f, -0.22f}, d.accent);
        m.addBox({-hw + 0.05f, 0.40f, -0.40f}, {-hw + 0.25f, 0.65f, 0.40f}, d.accent);
        m.addBox({hw - 0.25f, 0.40f, -0.40f}, {hw - 0.05f, 0.65f, 0.40f}, d.accent);
        break;
    }
    default:
        m.addBox({-hw + 0.1f, 0, -hd + 0.1f}, {hw - 0.1f, 0.6f, hd - 0.1f}, d.main);
    }
    return m;
}

} // namespace hotel
