#pragma once
#include <android/asset_manager.h>
#include "assets/AssetIO.h"

namespace hotel {

// Membaca dari app/src/main/assets/ lewat AAssetManager.
class AndroidAssetIO : public AssetIO {
public:
    explicit AndroidAssetIO(AAssetManager* mgr) : mgr_(mgr) {}
    bool read(const std::string& path, std::vector<uint8_t>& out) override {
        if (!mgr_) return false;
        AAsset* a = AAssetManager_open(mgr_, path.c_str(), AASSET_MODE_BUFFER);
        if (!a) return false;
        off_t len = AAsset_getLength(a);
        out.resize(static_cast<size_t>(len));
        int n = AAsset_read(a, out.data(), static_cast<size_t>(len));
        AAsset_close(a);
        return n == len;
    }
private:
    AAssetManager* mgr_;
};

} // namespace hotel
