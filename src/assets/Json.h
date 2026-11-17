#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace hotel {

// JSON parser kecil (cukup untuk glTF).
struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    double num = 0;
    bool b = false;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::string> keys;
    std::vector<JsonValue> vals;

    bool isNull() const { return type == Null; }
    size_t size() const { return type == Array ? arr.size() : vals.size(); }
    const JsonValue& operator[](size_t i) const { return i < arr.size() ? arr[i] : null(); }
    const JsonValue& operator[](const std::string& k) const {
        for (size_t i = 0; i < keys.size(); ++i) if (keys[i] == k) return vals[i];
        return null();
    }
    bool has(const std::string& k) const { return !(*this)[k].isNull(); }
    double number(double def = 0) const { return type == Number ? num : def; }
    int integer(int def = 0) const { return type == Number ? static_cast<int>(num) : def; }
    const std::string& string() const { return str; }

    static const JsonValue& null() { static const JsonValue n; return n; }
};

bool parseJson(const char* data, size_t len, JsonValue& out);

} // namespace hotel
