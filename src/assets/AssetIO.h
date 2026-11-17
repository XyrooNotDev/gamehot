#pragma once
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace hotel {

// Abstraksi baca file aset. Android: AAssetManager (folder app/src/main/assets). Desktop/tes: file biasa.
class AssetIO {
public:
    virtual ~AssetIO() = default;
    virtual bool read(const std::string& path, std::vector<uint8_t>& out) = 0;
    bool exists(const std::string& path) { std::vector<uint8_t> tmp; return read(path, tmp); }
};

class FileAssetIO : public AssetIO {
public:
    explicit FileAssetIO(std::string root) : root_(std::move(root)) {}
    bool read(const std::string& path, std::vector<uint8_t>& out) override {
        std::ifstream f(root_ + "/" + path, std::ios::binary);
        if (!f) return false;
        out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }
private:
    std::string root_;
};

} // namespace hotel
