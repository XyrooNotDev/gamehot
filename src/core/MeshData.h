#pragma once
#include <cstdint>
#include <vector>
#include "core/Math.h"

namespace hotel {

// Mesh di sisi CPU (tanpa dependensi OpenGL) -> mudah dites di desktop.
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
};

struct MeshData {
    enum Face : uint8_t {
        PosX = 1, NegX = 2, PosY = 4, NegY = 8, PosZ = 16, NegZ = 32, All = 63
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // p0..p3 harus berurutan CCW dilihat dari sisi normal.
    void addQuad(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 n, Color c) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        for (Vec3 p : {p0, p1, p2, p3})
            vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z, c.r, c.g, c.b});
        for (uint32_t i : {0u, 1u, 2u, 0u, 2u, 3u}) indices.push_back(base + i);
    }

    void addBox(Vec3 mn, Vec3 mx, Color c, uint8_t faces = All) {
        const float x0 = mn.x, y0 = mn.y, z0 = mn.z, x1 = mx.x, y1 = mx.y, z1 = mx.z;
        if (faces & PosY) addQuad({x0,y1,z1},{x1,y1,z1},{x1,y1,z0},{x0,y1,z0},{0,1,0},c);
        if (faces & NegY) addQuad({x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1},{0,-1,0},c);
        if (faces & PosZ) addQuad({x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1},{0,0,1},c);
        if (faces & NegZ) addQuad({x1,y0,z0},{x0,y0,z0},{x0,y1,z0},{x1,y1,z0},{0,0,-1},c);
        if (faces & PosX) addQuad({x1,y0,z1},{x1,y0,z0},{x1,y1,z0},{x1,y1,z1},{1,0,0},c);
        if (faces & NegX) addQuad({x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0},{-1,0,0},c);
    }
};

} // namespace hotel
