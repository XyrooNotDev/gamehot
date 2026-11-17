#pragma once
#include <cmath>
#include <cstdint>

namespace hotel {

constexpr float kPi = 3.14159265358979f;

struct Vec2 { float x = 0, y = 0; };

struct IVec2 {
    int x = 0, y = 0;
    bool operator==(IVec2 o) const { return x == o.x && y == o.y; }
    bool operator!=(IVec2 o) const { return !(*this == o); }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalize(Vec3 v) {
    float l = std::sqrt(dot(v, v));
    return l > 1e-8f ? v * (1.0f / l) : Vec3{};
}

struct Color { float r = 1, g = 1, b = 1, a = 1; };

// Matriks column-major (langsung kompatibel dengan OpenGL): m[kolom * 4 + baris]
struct Mat4 {
    float m[16] = {};

    static Mat4 identity() {
        Mat4 r;
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static Mat4 translation(Vec3 t) {
        Mat4 r = identity();
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }
    static Mat4 rotationY(float a) {
        Mat4 r = identity();
        float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[8] = s; r.m[2] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 perspective(float fovY, float aspect, float zn, float zf) {
        Mat4 r;
        float f = 1.0f / std::tan(fovY * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zf + zn) / (zn - zf);
        r.m[11] = -1.0f;
        r.m[14] = 2.0f * zf * zn / (zn - zf);
        return r;
    }
    static Mat4 lookAt(Vec3 eye, Vec3 target, Vec3 up) {
        Vec3 f = normalize(target - eye);
        Vec3 s = normalize(cross(f, up));
        Vec3 u = cross(s, f);
        Mat4 r = identity();
        r.m[0] = s.x; r.m[4] = s.y; r.m[8]  = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9]  = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] = dot(f, eye);
        return r;
    }
    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + row] * b.m[c * 4 + k];
                r.m[c * 4 + row] = s;
            }
        return r;
    }
};

// Proyeksi titik dunia ke piksel layar (origin kiri-atas). false jika di belakang kamera.
inline bool projectToScreen(Vec3 p, const Mat4& vp, float w, float h, Vec2& out) {
    const float* m = vp.m;
    float cx = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    float cy = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    float cw = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    if (cw <= 0.001f) return false;
    out.x = (cx / cw * 0.5f + 0.5f) * w;
    out.y = (1.0f - (cy / cw * 0.5f + 0.5f)) * h;
    return true;
}

} // namespace hotel
