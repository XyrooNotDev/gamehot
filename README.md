# Hotel Tycoon (C++ / OpenGL ES 3 / Android)

Game hotel 3D mobile: bangun hotel, beli furnitur, layani tamu, kumpulkan uang.

## Cara main
- **Joystick** (separuh kiri layar): jalan. **Geser** di separuh kanan: putar kamera.
- **BELI**: buka toko -> pilih furnitur -> pratinjau muncul di depan karakter (hijau = valid,
  merah = tidak) -> **PUTAR** / **OKE** / **BATAL**.
- Tamu datang ke pintu masuk dan antre di resepsionis (tanda "!" + bar kesabaran).
  Dekati meja resepsionis lalu tekan **CHECK-IN**; tamu diantar ke kasur kosong, tidur,
  lalu membayar saat pulang (suara koin, "+$").
- Pendapatan = tarif kasur + bonus dekorasi kamar (+8 per dekor, maks 5) + tip lobi (+2 per dekor, maks 10).
- **JUAL**: berdiri menghadap furnitur -> tombol JUAL (kembali 50%).
- Tamu yang menunggu terlalu lama pergi marah. Data otomatis tersimpan saat app di-pause.

## Layar loading
Saat app dibuka tampil layar pembuka (gedung hotel dengan jendela berkedip, judul, bar progres, tips).
Aset dimuat bertahap satu langkah per frame (suara -> denah -> karakter -> tiap furnitur), jadi bar
benar-benar mengikuti proses; minimal tampil ~1.6 detik lalu memudar ke permainan. Latar jendela Android
memakai warna yang sama (`res/values/styles.xml`) sehingga tidak ada kilatan putih saat peluncuran.
Jika konteks GL hilang (app dipindah-pindah) loading singkat tampil lagi, uang & furnitur tetap.

## Build lewat GitHub Actions (.github/workflows/android.yml)
Push proyek ini ke GitHub -> tab **Actions** -> "Build Hotel Tycoon". Tiga job:
1. **Tes desktop** - cmake + ctest dengan AddressSanitizer/UBSan.
2. **APK debug** - hasilnya ada di *Artifacts* (`HotelTycoon-debug-apk`), langsung bisa di-install.
3. **APK release** - hanya saat push tag `v*` (mis. `git tag v0.2 && git push --tags`); otomatis dibuat GitHub Release.
   Tanda tangan opsional: isi secret `KEYSTORE_BASE64` (hasil `base64 -w0 my.keystore`), `KEYSTORE_PASSWORD`,
   `KEY_ALIAS`, `KEY_PASSWORD`. Tanpa secret, APK release tidak ditandatangani (tidak bisa di-install langsung).
Pemicu manual: tombol "Run workflow". Versi alat (NDK 26.1.10909125, CMake 3.22.1, Gradle 8.7, JDK 17) ada di bagian `env`.

## Build
1. Android Studio -> Open folder ini (butuh Android SDK 34, NDK, CMake 3.22 dari SDK Manager).
2. Run di HP / emulator (Android 8.0+ / API 26, GLES 3.0). Butuh Gradle 8.7+ (Studio menawarkan sinkronisasi).

## Tes di desktop (tanpa GL/Android)
    cmake -S . -B build -DHOTEL_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
Tanpa CMake:
    g++ -std=c++17 -Isrc -DTEST_DATA_DIR='"tests/data"' tests/test_all.cpp tests/RendererStub.cpp \
        src/{world,furniture,npc,game,ui,audio,assets}/*.cpp -o test_all && ./test_all

## Struktur
    src/core       Math, MeshData, Events (tanpa GL)
    src/world      HotelMap (grid, region, entrance), RoomBuilder (prosedural), Pathfinder (A*)
    src/furniture  FurnitureCatalog, FurnitureManager (penempatan, validasi, jual)
    src/npc        GuestAI (GuestManager: datang -> antre -> check-in -> kamar -> bayar -> pulang)
    src/game       Game (penghubung), Player, CameraController, Economy
    src/renderer   Renderer GLES3
    src/ui         TouchUI (joystick, tombol, panel), UIBatch, Font bitmap 5x7
    src/audio      SoundBank (suara sintetis + WAV), Mixer (lock-free)
    src/assets     AssetLoader (glTF/GLB/OBJ), Texture (PNG+inflate), Json, AssetIO
    src/platform/android   main (EGL, input, simpan), AudioOutput (AAudio), AndroidAssetIO
    tests          tes + fixture

## Mengubah denah
Edit `kMapRows` di `src/game/Game.cpp` ('#' dinding, '.' lantai, 'D' pintu, 'E' pintu masuk,
'P' spawn). Region (lobi/kamar) dihitung otomatis; kasur hanya boleh di kamar, resepsionis di lobi.

Aset: lihat ASSETS.md.
