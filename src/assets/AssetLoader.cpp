#include "assets/AssetLoader.h"
#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include "assets/Json.h"
#include "assets/Texture.h"

namespace hotel {

namespace {

std::string dirOf(const std::string& p) {
    size_t s = p.find_last_of('/');
    return s == std::string::npos ? std::string() : p.substr(0, s + 1);
}

Color mul(Color a, Color b) { return {a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a}; }

// ---------- base64 ----------
bool base64Decode(const std::string& in, std::vector<uint8_t>& out) {
    static int8_t T[256];
    static bool init = false;
    if (!init) {
        std::memset(T, -1, sizeof(T));
        const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) T[static_cast<unsigned char>(a[i])] = static_cast<int8_t>(i);
        init = true;
    }
    uint32_t buf = 0; int bits = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        int v = T[c];
        if (v < 0) continue;
        buf = (buf << 6) | v; bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF)); }
    }
    return true;
}

// ---------- glTF ----------
struct GltfCtx {
    AssetIO* io;
    std::string baseDir;
    JsonValue j;
    std::vector<std::vector<uint8_t>> buffers;
    std::vector<Texture> images;
    std::vector<bool> imageLoaded;
};

bool loadUri(GltfCtx& c, const std::string& uri, std::vector<uint8_t>& out) {
    if (uri.compare(0, 5, "data:") == 0) {
        size_t comma = uri.find(',');
        return comma != std::string::npos && base64Decode(uri.substr(comma + 1), out);
    }
    return c.io && c.io->read(c.baseDir + uri, out);
}

struct Accessor {
    const uint8_t* base = nullptr;
    size_t count = 0, stride = 0;
    int comps = 0, ctype = 0;
    bool normalized = false;
    bool valid() const { return base != nullptr; }

    float f(size_t i, int k) const {
        const uint8_t* p = base + i * stride;
        switch (ctype) {
            case 5126: { float v; std::memcpy(&v, p + k * 4, 4); return v; }
            case 5121: return normalized ? p[k] / 255.0f : p[k];
            case 5123: { uint16_t v; std::memcpy(&v, p + k * 2, 2); return normalized ? v / 65535.0f : v; }
            case 5120: return normalized ? std::max(-1.0f, static_cast<int8_t>(p[k]) / 127.0f) : static_cast<int8_t>(p[k]);
            case 5122: { int16_t v; std::memcpy(&v, p + k * 2, 2); return normalized ? std::max(-1.0f, v / 32767.0f) : v; }
            default: return 0;
        }
    }
    uint32_t u(size_t i) const {
        const uint8_t* p = base + i * stride;
        switch (ctype) {
            case 5121: return p[0];
            case 5123: { uint16_t v; std::memcpy(&v, p, 2); return v; }
            case 5125: { uint32_t v; std::memcpy(&v, p, 4); return v; }
            default: return 0;
        }
    }
};

Accessor getAccessor(GltfCtx& c, int idx) {
    Accessor a;
    const JsonValue& acc = c.j["accessors"][static_cast<size_t>(idx)];
    if (acc.isNull() || !acc.has("bufferView")) return a;
    const JsonValue& bv = c.j["bufferViews"][static_cast<size_t>(acc["bufferView"].integer())];
    int bi = bv["buffer"].integer();
    if (bi < 0 || bi >= static_cast<int>(c.buffers.size())) return a;
    const auto& buf = c.buffers[bi];
    static const int kComps[] = {1, 2, 3, 4, 4, 9, 16};
    static const char* kTypes[] = {"SCALAR", "VEC2", "VEC3", "VEC4", "MAT2", "MAT3", "MAT4"};
    for (int i = 0; i < 7; ++i) if (acc["type"].string() == kTypes[i]) a.comps = kComps[i];
    a.ctype = acc["componentType"].integer();
    a.normalized = acc["normalized"].b;
    a.count = static_cast<size_t>(acc["count"].integer());
    int csz = (a.ctype == 5126 || a.ctype == 5125) ? 4 : (a.ctype == 5123 || a.ctype == 5122) ? 2 : 1;
    a.stride = bv.has("byteStride") ? static_cast<size_t>(bv["byteStride"].integer()) : static_cast<size_t>(a.comps * csz);
    size_t off = static_cast<size_t>(bv["byteOffset"].integer()) + static_cast<size_t>(acc["byteOffset"].integer());
    if (a.comps == 0 || off + (a.count ? (a.count - 1) * a.stride + a.comps * csz : 0) > buf.size()) return Accessor{};
    a.base = buf.data() + off;
    return a;
}

const Texture* getImage(GltfCtx& c, int texIndex) {
    if (texIndex < 0) return nullptr;
    int src = c.j["textures"][static_cast<size_t>(texIndex)]["source"].integer(-1);
    if (src < 0 || src >= static_cast<int>(c.images.size())) return nullptr;
    if (!c.imageLoaded[src]) {
        c.imageLoaded[src] = true;
        const JsonValue& im = c.j["images"][static_cast<size_t>(src)];
        std::vector<uint8_t> bytes;
        if (im.has("uri")) loadUri(c, im["uri"].string(), bytes);
        else if (im.has("bufferView")) {
            const JsonValue& bv = c.j["bufferViews"][static_cast<size_t>(im["bufferView"].integer())];
            int bi = bv["buffer"].integer();
            size_t off = static_cast<size_t>(bv["byteOffset"].integer()), len = static_cast<size_t>(bv["byteLength"].integer());
            if (bi >= 0 && bi < static_cast<int>(c.buffers.size()) && off + len <= c.buffers[bi].size())
                bytes.assign(c.buffers[bi].begin() + off, c.buffers[bi].begin() + off + len);
        }
        if (!bytes.empty()) decodePng(bytes.data(), bytes.size(), c.images[src]);   // JPEG tidak didukung -> pakai warna dasar
    }
    return c.images[src].valid() ? &c.images[src] : nullptr;
}

Mat4 nodeMatrix(const JsonValue& n) {
    if (n.has("matrix") && n["matrix"].size() == 16) {
        Mat4 m;
        for (int i = 0; i < 16; ++i) m.m[i] = static_cast<float>(n["matrix"][static_cast<size_t>(i)].number());
        return m;
    }
    Vec3 t{0, 0, 0}, s{1, 1, 1};
    float q[4] = {0, 0, 0, 1};
    if (n.has("translation")) t = {float(n["translation"][0].number()), float(n["translation"][1].number()), float(n["translation"][2].number())};
    if (n.has("scale")) s = {float(n["scale"][0].number(1)), float(n["scale"][1].number(1)), float(n["scale"][2].number(1))};
    if (n.has("rotation")) for (int i = 0; i < 4; ++i) q[i] = static_cast<float>(n["rotation"][static_cast<size_t>(i)].number());
    float x = q[0], y = q[1], z = q[2], w = q[3];
    Mat4 m = Mat4::identity();
    m.m[0] = (1 - 2 * (y * y + z * z)) * s.x; m.m[1] = (2 * (x * y + z * w)) * s.x;     m.m[2] = (2 * (x * z - y * w)) * s.x;
    m.m[4] = (2 * (x * y - z * w)) * s.y;     m.m[5] = (1 - 2 * (x * x + z * z)) * s.y; m.m[6] = (2 * (y * z + x * w)) * s.y;
    m.m[8] = (2 * (x * z + y * w)) * s.z;     m.m[9] = (2 * (y * z - x * w)) * s.z;     m.m[10] = (1 - 2 * (x * x + y * y)) * s.z;
    m.m[12] = t.x; m.m[13] = t.y; m.m[14] = t.z;
    return m;
}

Vec3 xformPoint(const Mat4& m, Vec3 p) {
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}
Vec3 xformDir(const Mat4& m, Vec3 p) {
    return normalize({m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z,
                      m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z,
                      m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z});
}

void emitMesh(GltfCtx& c, int meshIdx, const Mat4& world, MeshData& out) {
    const JsonValue& mesh = c.j["meshes"][static_cast<size_t>(meshIdx)];
    const JsonValue& prims = mesh["primitives"];
    for (size_t pi = 0; pi < prims.size(); ++pi) {
        const JsonValue& prim = prims[pi];
        if (prim.has("mode") && prim["mode"].integer() != 4) continue;   // hanya segitiga
        Accessor pos = getAccessor(c, prim["attributes"]["POSITION"].integer(-1));
        if (!pos.valid() || pos.comps < 3) continue;
        Accessor nor = prim["attributes"].has("NORMAL") ? getAccessor(c, prim["attributes"]["NORMAL"].integer()) : Accessor{};
        Accessor uv = prim["attributes"].has("TEXCOORD_0") ? getAccessor(c, prim["attributes"]["TEXCOORD_0"].integer()) : Accessor{};
        Accessor col = prim["attributes"].has("COLOR_0") ? getAccessor(c, prim["attributes"]["COLOR_0"].integer()) : Accessor{};

        Color base{1, 1, 1, 1};
        const Texture* tex = nullptr;
        if (prim.has("material")) {
            const JsonValue& pbr = c.j["materials"][static_cast<size_t>(prim["material"].integer())]["pbrMetallicRoughness"];
            if (pbr.has("baseColorFactor"))
                base = {float(pbr["baseColorFactor"][0].number(1)), float(pbr["baseColorFactor"][1].number(1)),
                        float(pbr["baseColorFactor"][2].number(1)), float(pbr["baseColorFactor"][3].number(1))};
            if (pbr.has("baseColorTexture")) tex = getImage(c, pbr["baseColorTexture"]["index"].integer(-1));
        }

        uint32_t first = static_cast<uint32_t>(out.vertices.size());
        for (size_t i = 0; i < pos.count; ++i) {
            Vec3 p = xformPoint(world, {pos.f(i, 0), pos.f(i, 1), pos.f(i, 2)});
            Vec3 n = nor.valid() ? xformDir(world, {nor.f(i, 0), nor.f(i, 1), nor.f(i, 2)}) : Vec3{0, 1, 0};
            Color cl = base;
            if (col.valid() && col.comps >= 3) cl = mul(cl, {col.f(i, 0), col.f(i, 1), col.f(i, 2), 1});
            if (tex && uv.valid()) cl = mul(cl, tex->sample(uv.f(i, 0), uv.f(i, 1)));
            out.vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z, cl.r, cl.g, cl.b});
        }

        std::vector<uint32_t> idx;
        if (prim.has("indices")) {
            Accessor ia = getAccessor(c, prim["indices"].integer());
            for (size_t i = 0; ia.valid() && i < ia.count; ++i) idx.push_back(ia.u(i));
        } else for (size_t i = 0; i < pos.count; ++i) idx.push_back(static_cast<uint32_t>(i));

        bool flip = (world.m[0] * (world.m[5] * world.m[10] - world.m[6] * world.m[9]) -
                     world.m[4] * (world.m[1] * world.m[10] - world.m[2] * world.m[9]) +
                     world.m[8] * (world.m[1] * world.m[6] - world.m[2] * world.m[5])) < 0;   // skala negatif membalik winding
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            if (idx[i] >= pos.count || idx[i + 1] >= pos.count || idx[i + 2] >= pos.count) continue;
            out.indices.push_back(first + idx[i]);
            out.indices.push_back(first + idx[flip ? i + 2 : i + 1]);
            out.indices.push_back(first + idx[flip ? i + 1 : i + 2]);
        }
        if (!nor.valid()) {   // normal datar jika model tanpa normal
            for (size_t i = first; i < out.vertices.size(); ++i) out.vertices[i].ny = 1;
        }
    }
}

void walkNode(GltfCtx& c, int ni, const Mat4& parent, MeshData& out, int depth) {
    if (depth > 32) return;
    const JsonValue& n = c.j["nodes"][static_cast<size_t>(ni)];
    if (n.isNull()) return;
    Mat4 world = parent * nodeMatrix(n);
    if (n.has("mesh")) emitMesh(c, n["mesh"].integer(), world, out);
    for (size_t i = 0; i < n["children"].size(); ++i) walkNode(c, n["children"][i].integer(), world, out, depth + 1);
}

} // namespace

bool AssetLoader::loadGltfMemory(const std::vector<uint8_t>& bytes, const std::string& baseDir, MeshData& out) {
    GltfCtx c;
    c.io = io_;
    c.baseDir = baseDir;
    std::vector<uint8_t> glbBin;
    const char* jsonPtr = nullptr;
    size_t jsonLen = 0;

    auto u32 = [&](size_t o) { return uint32_t(bytes[o]) | (uint32_t(bytes[o + 1]) << 8) | (uint32_t(bytes[o + 2]) << 16) | (uint32_t(bytes[o + 3]) << 24); };
    if (bytes.size() >= 20 && u32(0) == 0x46546C67) {   // "glTF"
        size_t p = 12;
        while (p + 8 <= bytes.size()) {
            uint32_t len = u32(p), type = u32(p + 4);
            if (p + 8 + len > bytes.size()) return false;
            if (type == 0x4E4F534A) { jsonPtr = reinterpret_cast<const char*>(&bytes[p + 8]); jsonLen = len; }
            else if (type == 0x004E4942) glbBin.assign(bytes.begin() + p + 8, bytes.begin() + p + 8 + len);
            p += 8 + len;
        }
    } else {
        jsonPtr = reinterpret_cast<const char*>(bytes.data());
        jsonLen = bytes.size();
    }
    if (!jsonPtr || !parseJson(jsonPtr, jsonLen, c.j)) return false;

    const JsonValue& bufs = c.j["buffers"];
    c.buffers.resize(bufs.size());
    for (size_t i = 0; i < bufs.size(); ++i) {
        if (bufs[i].has("uri")) { if (!loadUri(c, bufs[i]["uri"].string(), c.buffers[i])) return false; }
        else if (i == 0) c.buffers[i] = glbBin;
    }
    c.images.resize(c.j["images"].size());
    c.imageLoaded.assign(c.images.size(), false);

    out = MeshData{};
    const JsonValue& scenes = c.j["scenes"];
    if (scenes.size() > 0) {
        int sc = c.j["scene"].integer(0);
        const JsonValue& roots = scenes[static_cast<size_t>(sc)]["nodes"];
        for (size_t i = 0; i < roots.size(); ++i) walkNode(c, roots[i].integer(), Mat4::identity(), out, 0);
    } else if (c.j["nodes"].size() > 0) {
        std::vector<bool> isChild(c.j["nodes"].size(), false);
        for (size_t i = 0; i < c.j["nodes"].size(); ++i)
            for (size_t k = 0; k < c.j["nodes"][i]["children"].size(); ++k) {
                int ch = c.j["nodes"][i]["children"][k].integer(-1);
                if (ch >= 0 && ch < static_cast<int>(isChild.size())) isChild[ch] = true;
            }
        for (size_t i = 0; i < isChild.size(); ++i) if (!isChild[i]) walkNode(c, static_cast<int>(i), Mat4::identity(), out, 0);
    } else {
        for (size_t i = 0; i < c.j["meshes"].size(); ++i) emitMesh(c, static_cast<int>(i), Mat4::identity(), out);
    }
    return !out.vertices.empty() && !out.indices.empty();
}

bool AssetLoader::loadGltfFile(const std::string& path, MeshData& out) {
    std::vector<uint8_t> bytes;
    if (!io_ || !io_->read(path, bytes)) return false;
    return loadGltfMemory(bytes, dirOf(path), out);
}

// ---------- OBJ ----------
bool AssetLoader::loadObjFile(const std::string& path, MeshData& out) {
    std::vector<uint8_t> bytes;
    if (!io_ || !io_->read(path, bytes)) return false;
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    const std::string dir = dirOf(path);

    struct Mat { Color kd{0.8f, 0.8f, 0.8f, 1}; Texture tex; };
    std::map<std::string, Mat> mats;
    std::vector<Vec3> P, N;
    std::vector<Vec2> T;
    Mat current;
    bool haveCurrent = false;
    out = MeshData{};

    auto loadMtl = [&](const std::string& file) {
        std::vector<uint8_t> mb;
        if (!io_->read(dir + file, mb)) return;
        std::istringstream ms(std::string(mb.begin(), mb.end()));
        std::string line, name;
        while (std::getline(ms, line)) {
            std::istringstream ls(line);
            std::string tag;
            ls >> tag;
            if (tag == "newmtl") { ls >> name; mats[name] = Mat{}; }
            else if (tag == "Kd" && !name.empty()) { ls >> mats[name].kd.r >> mats[name].kd.g >> mats[name].kd.b; }
            else if (tag == "map_Kd" && !name.empty()) {
                std::string tf; ls >> tf;
                std::vector<uint8_t> pb;
                if (io_->read(dir + tf, pb)) decodePng(pb.data(), pb.size(), mats[name].tex);
            }
        }
    };

    auto resolve = [](int i, size_t n) { return i > 0 ? i - 1 : static_cast<int>(n) + i; };

    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string tag;
        ls >> tag;
        if (tag == "v") { Vec3 v; ls >> v.x >> v.y >> v.z; P.push_back(v); }
        else if (tag == "vn") { Vec3 v; ls >> v.x >> v.y >> v.z; N.push_back(v); }
        else if (tag == "vt") { Vec2 v; ls >> v.x >> v.y; T.push_back(v); }
        else if (tag == "mtllib") { std::string f; ls >> f; loadMtl(f); }
        else if (tag == "usemtl") {
            std::string name; ls >> name;
            auto it = mats.find(name);
            if (it != mats.end()) { current = it->second; haveCurrent = true; } else haveCurrent = false;
        }
        else if (tag == "f") {
            struct Corner { int v = -1, t = -1, n = -1; };
            std::vector<Corner> cs;
            std::string tok;
            while (ls >> tok) {
                Corner c;
                int a = 0, b = 0, d = 0;
                size_t s1 = tok.find('/');
                if (s1 == std::string::npos) a = std::atoi(tok.c_str());
                else {
                    a = std::atoi(tok.substr(0, s1).c_str());
                    size_t s2 = tok.find('/', s1 + 1);
                    if (s2 == std::string::npos) b = std::atoi(tok.substr(s1 + 1).c_str());
                    else { if (s2 > s1 + 1) b = std::atoi(tok.substr(s1 + 1, s2 - s1 - 1).c_str()); d = std::atoi(tok.substr(s2 + 1).c_str()); }
                }
                if (a) c.v = resolve(a, P.size());
                if (b) c.t = resolve(b, T.size());
                if (d) c.n = resolve(d, N.size());
                if (c.v < 0 || c.v >= static_cast<int>(P.size())) { cs.clear(); break; }
                cs.push_back(c);
            }
            if (cs.size() < 3) continue;
            Vec3 fn = normalize(cross(P[cs[1].v] - P[cs[0].v], P[cs[2].v] - P[cs[0].v]));
            for (size_t i = 1; i + 1 < cs.size(); ++i) {
                const Corner* tri[3] = {&cs[0], &cs[i], &cs[i + 1]};
                for (const Corner* c : tri) {
                    Vec3 n = (c->n >= 0 && c->n < static_cast<int>(N.size())) ? normalize(N[c->n]) : fn;
                    Color col = haveCurrent ? current.kd : Color{0.8f, 0.8f, 0.8f, 1};
                    if (haveCurrent && current.tex.valid() && c->t >= 0 && c->t < static_cast<int>(T.size()))
                        col = mul(col.r + col.g + col.b < 0.01f ? Color{1, 1, 1, 1} : col, current.tex.sample(T[c->t].x, 1.0f - T[c->t].y));
                    out.indices.push_back(static_cast<uint32_t>(out.vertices.size()));
                    out.vertices.push_back({P[c->v].x, P[c->v].y, P[c->v].z, n.x, n.y, n.z, col.r, col.g, col.b});
                }
            }
        }
    }
    return !out.vertices.empty();
}

bool AssetLoader::loadModel(const std::string& base, MeshData& out) {
    if (!io_) return false;
    return loadGltfFile(base + ".glb", out) || loadGltfFile(base + ".gltf", out) || loadObjFile(base + ".obj", out);
}

void AssetLoader::fit(MeshData& m, float fw, float fd, float heightTarget) {
    if (m.vertices.empty()) return;
    Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (const auto& v : m.vertices) {
        mn = {std::min(mn.x, v.px), std::min(mn.y, v.py), std::min(mn.z, v.pz)};
        mx = {std::max(mx.x, v.px), std::max(mx.y, v.py), std::max(mx.z, v.pz)};
    }
    float ex = std::max(1e-4f, mx.x - mn.x), ey = std::max(1e-4f, mx.y - mn.y), ez = std::max(1e-4f, mx.z - mn.z);
    float s = heightTarget > 0 ? heightTarget / ey : std::min(fw * 0.92f / ex, fd * 0.92f / ez);
    float cx = (mn.x + mx.x) * 0.5f, cz = (mn.z + mx.z) * 0.5f;
    for (auto& v : m.vertices) {
        v.px = (v.px - cx) * s;
        v.py = (v.py - mn.y) * s;
        v.pz = (v.pz - cz) * s;
    }
}

} // namespace hotel
