# Asset pack - Hotel Tycoon

## Audio (bundled)
Folder `app/src/main/assets/sounds/` contains six original WAV effects:
`bell.wav`, `footstep.wav`, `coin.wav`, `click.wav`, `place.wav`, `error.wav`.
They are PCM16 mono at 44.1 kHz and are loaded by `Game::runLoadStep`; built-in
synthesized sounds remain as fallback if a file is missing or invalid.

## 3D models (runtime fallback)
Folder `app/src/main/assets/models/` documents supported model names. This project
currently builds furniture, rooms, player and guest meshes procedurally in C++ when
optional external models are absent. This is intentional and keeps the APK self-contained.
To replace a procedural mesh, add a compatible GLB/glTF or OBJ asset using the names
listed in `app/src/main/assets/models/README.txt`.

## Supported formats and limitations
- glTF 2.0 `.glb` / `.gltf` and OBJ/MTL are supported by the lightweight loader.
- PNG base-color textures are supported; JPEG textures, skeletal animation and skinning
  are not implemented.
- Keep each added model lightweight and verify its license before distribution.

## Audio limitations
The current mixer plays sound effects. Background music playback is not wired into the
runtime yet; do not place a music file in the assets folder expecting it to play.


## Realistic GLB integration scaffold
The runtime model slots and their exact names are documented in
`app/src/main/assets/models/README.txt`; free candidate sources and license
notes are in `ASSET_SOURCES.md`. The current loader already reads those names
through `AndroidAssetIO`, so adding a compatible self-contained GLB requires
no CMake or Gradle registration. No third-party GLB binaries are included in
this archive because the source download endpoints were not retrievable in
this build session; the existing procedural meshes remain active until the
corresponding licensed files are added.

## Launcher mipmap
Launcher icon resources are now provided under `res/mipmap-*` for legacy
density scaling and `res/mipmap-anydpi-v26` for adaptive icons. The manifest
references `@mipmap/ic_launcher`.
