#include "assets/Texture.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace hotel {

Color Texture::sample(float u, float v) const {
    if (!valid()) return {1, 1, 1, 1};
    u -= std::floor(u); v -= std::floor(v);
    int x = std::min(w - 1, static_cast<int>(u * w));
    int y = std::min(h - 1, static_cast<int>(v * h));
    const uint8_t* p = &rgba[(static_cast<size_t>(y) * w + x) * 4];
    return {p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, p[3] / 255.0f};
}

namespace {

struct Bits {
    const uint8_t* d; size_t n, pos = 0; uint32_t buf = 0; int cnt = 0; bool err = false;
    uint32_t get(int need) {
        while (cnt < need) {
            if (pos >= n) { err = true; return 0; }
            buf |= static_cast<uint32_t>(d[pos++]) << cnt;
            cnt += 8;
        }
        uint32_t v = buf & ((1u << need) - 1);
        buf >>= need; cnt -= need;
        return v;
    }
};

struct Huff { uint16_t count[16]; uint16_t symbol[320]; };

void buildHuff(Huff& h, const uint8_t* len, int n) {
    std::memset(h.count, 0, sizeof(h.count));
    for (int i = 0; i < n; ++i) h.count[len[i]]++;
    uint16_t offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; ++i) offs[i + 1] = offs[i] + h.count[i];
    for (int i = 0; i < n; ++i) if (len[i]) h.symbol[offs[len[i]]++] = static_cast<uint16_t>(i);
}

int decodeSym(Bits& b, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; ++len) {
        code |= static_cast<int>(b.get(1));
        if (b.err) return -1;
        int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count; first += count; first <<= 1; code <<= 1;
    }
    return -1;
}

const uint16_t kLenBase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const uint16_t kLenExt[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const uint16_t kDistBase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const uint16_t kDistExt[30]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

bool inflateCodes(Bits& b, std::vector<uint8_t>& out, const Huff& lit, const Huff& dist) {
    for (;;) {
        int sym = decodeSym(b, lit);
        if (sym < 0) return false;
        if (sym < 256) out.push_back(static_cast<uint8_t>(sym));
        else if (sym == 256) return true;
        else {
            sym -= 257;
            if (sym >= 29) return false;
            int len = kLenBase[sym] + static_cast<int>(b.get(kLenExt[sym]));
            int ds = decodeSym(b, dist);
            if (ds < 0 || ds >= 30) return false;
            size_t d = kDistBase[ds] + b.get(kDistExt[ds]);
            if (b.err || d > out.size()) return false;
            size_t from = out.size() - d;
            for (int i = 0; i < len; ++i) out.push_back(out[from + i]);
        }
    }
}

} // namespace

bool zlibInflate(const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    out.clear();
    if (size < 6) return false;
    Bits b{data + 2, size - 2};   // lewati header zlib 2 byte
    int last;
    do {
        last = static_cast<int>(b.get(1));
        int type = static_cast<int>(b.get(2));
        if (b.err) return false;
        if (type == 0) {
            b.buf = 0; b.cnt = 0;
            if (b.pos + 4 > b.n) return false;
            size_t len = b.d[b.pos] | (b.d[b.pos + 1] << 8);
            b.pos += 4;
            if (b.pos + len > b.n) return false;
            out.insert(out.end(), b.d + b.pos, b.d + b.pos + len);
            b.pos += len;
        } else if (type == 1) {
            uint8_t l[288];
            int i = 0;
            for (; i < 144; ++i) l[i] = 8;
            for (; i < 256; ++i) l[i] = 9;
            for (; i < 280; ++i) l[i] = 7;
            for (; i < 288; ++i) l[i] = 8;
            Huff lit, dist;
            buildHuff(lit, l, 288);
            uint8_t dl[30];
            std::memset(dl, 5, sizeof(dl));
            buildHuff(dist, dl, 30);
            if (!inflateCodes(b, out, lit, dist)) return false;
        } else if (type == 2) {
            int nlen = static_cast<int>(b.get(5)) + 257, ndist = static_cast<int>(b.get(5)) + 1, ncode = static_cast<int>(b.get(4)) + 4;
            if (b.err || nlen > 286 || ndist > 30) return false;
            static const uint8_t order[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
            uint8_t lengths[320] = {};
            for (int i = 0; i < ncode; ++i) lengths[order[i]] = static_cast<uint8_t>(b.get(3));
            Huff lc;
            buildHuff(lc, lengths, 19);
            int idx = 0;
            uint8_t ll[320] = {};
            while (idx < nlen + ndist) {
                int sym = decodeSym(b, lc);
                if (sym < 0) return false;
                if (sym < 16) ll[idx++] = static_cast<uint8_t>(sym);
                else {
                    int rep, val = 0;
                    if (sym == 16) { if (idx == 0) return false; val = ll[idx - 1]; rep = 3 + static_cast<int>(b.get(2)); }
                    else if (sym == 17) rep = 3 + static_cast<int>(b.get(3));
                    else rep = 11 + static_cast<int>(b.get(7));
                    if (idx + rep > nlen + ndist) return false;
                    while (rep--) ll[idx++] = static_cast<uint8_t>(val);
                }
            }
            Huff lit, dist;
            buildHuff(lit, ll, nlen);
            buildHuff(dist, ll + nlen, ndist);
            if (!inflateCodes(b, out, lit, dist)) return false;
        } else return false;
        if (b.err) return false;
    } while (!last);
    return true;
}

bool decodePng(const uint8_t* d, size_t n, Texture& tex) {
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (n < 33 || std::memcmp(d, sig, 8) != 0) return false;
    auto be32 = [&](size_t o) { return (uint32_t(d[o]) << 24) | (uint32_t(d[o + 1]) << 16) | (uint32_t(d[o + 2]) << 8) | d[o + 3]; };

    uint32_t w = 0, h = 0;
    int depth = 0, ctype = 0, interlace = 0;
    std::vector<uint8_t> idat, plte, trns;
    size_t p = 8;
    while (p + 12 <= n) {
        uint32_t len = be32(p);
        const uint8_t* type = d + p + 4;
        if (p + 12 + len > n) return false;
        const uint8_t* body = d + p + 8;
        if (!std::memcmp(type, "IHDR", 4) && len >= 13) {
            w = be32(p + 8); h = be32(p + 12);
            depth = body[8]; ctype = body[9]; interlace = body[12];
        } else if (!std::memcmp(type, "PLTE", 4)) plte.assign(body, body + len);
        else if (!std::memcmp(type, "tRNS", 4)) trns.assign(body, body + len);
        else if (!std::memcmp(type, "IDAT", 4)) idat.insert(idat.end(), body, body + len);
        else if (!std::memcmp(type, "IEND", 4)) break;
        p += 12 + len;
    }
    if (!w || !h || w > 8192 || h > 8192 || interlace != 0) return false;
    int channels;
    switch (ctype) { case 0: channels = 1; break; case 2: channels = 3; break; case 3: channels = 1; break; case 4: channels = 2; break; case 6: channels = 4; break; default: return false; }
    if (depth != 8 && !(ctype == 3 && (depth == 1 || depth == 2 || depth == 4))) return false;
    if (ctype == 3 && plte.empty()) return false;

    std::vector<uint8_t> raw;
    if (!zlibInflate(idat.data(), idat.size(), raw)) return false;
    size_t bitsPerPixel = static_cast<size_t>(channels) * depth;
    size_t stride = (w * bitsPerPixel + 7) / 8, bpp = std::max<size_t>(1, bitsPerPixel / 8);
    if (raw.size() < (stride + 1) * h) return false;

    std::vector<uint8_t> img(stride * h);
    for (uint32_t y = 0; y < h; ++y) {
        uint8_t f = raw[y * (stride + 1)];
        const uint8_t* src = &raw[y * (stride + 1) + 1];
        uint8_t* dst = &img[y * stride];
        const uint8_t* up = y ? &img[(y - 1) * stride] : nullptr;
        for (size_t i = 0; i < stride; ++i) {
            int a = i >= bpp ? dst[i - bpp] : 0, b = up ? up[i] : 0, c = (up && i >= bpp) ? up[i - bpp] : 0;
            int v = src[i];
            switch (f) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: { int pp = a + b - c, pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
                          v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c); break; }
                default: return false;
            }
            dst[i] = static_cast<uint8_t>(v);
        }
    }

    tex.w = static_cast<int>(w); tex.h = static_cast<int>(h);
    tex.rgba.assign(static_cast<size_t>(w) * h * 4, 255);
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t* row = &img[y * stride];
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t* o = &tex.rgba[(static_cast<size_t>(y) * w + x) * 4];
            switch (ctype) {
                case 0: o[0] = o[1] = o[2] = row[x]; break;
                case 2: o[0] = row[x * 3]; o[1] = row[x * 3 + 1]; o[2] = row[x * 3 + 2]; break;
                case 4: o[0] = o[1] = o[2] = row[x * 2]; o[3] = row[x * 2 + 1]; break;
                case 6: std::memcpy(o, row + x * 4, 4); break;
                case 3: {
                    int idx;
                    if (depth == 8) idx = row[x];
                    else { int per = 8 / depth, sh = 8 - depth * (static_cast<int>(x) % per + 1); idx = (row[x / per] >> sh) & ((1 << depth) - 1); }
                    if (static_cast<size_t>(idx) * 3 + 2 >= plte.size()) idx = 0;
                    o[0] = plte[idx * 3]; o[1] = plte[idx * 3 + 1]; o[2] = plte[idx * 3 + 2];
                    if (static_cast<size_t>(idx) < trns.size()) o[3] = trns[idx];
                    break;
                }
            }
        }
    }
    return true;
}

} // namespace hotel
