#include "assets/Json.h"
#include <cstdlib>
#include <cstring>

namespace hotel {

namespace {
struct Parser {
    const char* p; const char* end; bool ok = true; int depth = 0;

    void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    bool lit(const char* s) { size_t n = std::strlen(s); if (static_cast<size_t>(end - p) >= n && !std::strncmp(p, s, n)) { p += n; return true; } return false; }

    std::string str() {
        std::string s;
        ++p;  // "
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                ++p;
                switch (*p) {
                    case 'n': s += '\n'; break; case 't': s += '\t'; break; case 'r': s += '\r'; break;
                    case 'b': s += '\b'; break; case 'f': s += '\f'; break;
                    case 'u': {
                        if (end - p < 5) { ok = false; return s; }
                        unsigned cp = static_cast<unsigned>(std::strtoul(std::string(p + 1, 4).c_str(), nullptr, 16));
                        p += 4;
                        if (cp < 0x80) s += static_cast<char>(cp);
                        else if (cp < 0x800) { s += static_cast<char>(0xC0 | (cp >> 6)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                        else { s += static_cast<char>(0xE0 | (cp >> 12)); s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); s += static_cast<char>(0x80 | (cp & 0x3F)); }
                        break;
                    }
                    default: s += *p;
                }
                ++p;
            } else s += *p++;
        }
        if (p >= end) ok = false; else ++p;
        return s;
    }

    JsonValue value() {
        JsonValue v;
        if (++depth > 64) { ok = false; return v; }
        ws();
        if (p >= end) { ok = false; --depth; return v; }
        if (*p == '{') {
            v.type = JsonValue::Object; ++p; ws();
            if (p < end && *p == '}') ++p;
            else while (ok) {
                ws();
                if (p >= end || *p != '"') { ok = false; break; }
                std::string k = str(); ws();
                if (p >= end || *p != ':') { ok = false; break; }
                ++p;
                v.keys.push_back(k); v.vals.push_back(value()); ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; break; }
                ok = false;
            }
        } else if (*p == '[') {
            v.type = JsonValue::Array; ++p; ws();
            if (p < end && *p == ']') ++p;
            else while (ok) {
                v.arr.push_back(value()); ws();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; break; }
                ok = false;
            }
        } else if (*p == '"') { v.type = JsonValue::String; v.str = str(); }
        else if (lit("true")) { v.type = JsonValue::Bool; v.b = true; }
        else if (lit("false")) { v.type = JsonValue::Bool; }
        else if (lit("null")) {}
        else {
            char* e = nullptr;
            v.num = std::strtod(p, &e);
            if (e == p) ok = false; else { v.type = JsonValue::Number; p = e; }
        }
        --depth;
        return v;
    }
};
}

bool parseJson(const char* data, size_t len, JsonValue& out) {
    Parser ps{data, data + len};
    out = ps.value();
    return ps.ok;
}

} // namespace hotel
