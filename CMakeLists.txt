cmake_minimum_required(VERSION 3.22)
project(HotelTycoon LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HOTEL_BUILD_TESTS "Bangun tes di desktop (tanpa GL / Android)" OFF)

# ---- Inti game: tanpa dependensi platform / GL (bisa dites di desktop) ----
add_library(hotel_core STATIC
    src/world/HotelMap.cpp
    src/world/RoomBuilder.cpp
    src/world/Pathfinder.cpp
    src/furniture/FurnitureCatalog.cpp
    src/furniture/FurnitureManager.cpp
    src/npc/GuestAI.cpp
    src/game/Player.cpp
    src/game/CameraController.cpp
    src/game/Game.cpp
    src/ui/TouchUI.cpp
    src/ui/UIBatch.cpp
    src/ui/Font.cpp
    src/audio/SoundBank.cpp
    src/audio/Mixer.cpp
    src/assets/Json.cpp
    src/assets/Texture.cpp
    src/assets/AssetLoader.cpp)
target_include_directories(hotel_core PUBLIC src)

if(ANDROID)
    # Renderer GLES3 ikut di inti (Game memanggilnya); di desktop diganti RendererStub saat tes.
    target_sources(hotel_core PRIVATE src/renderer/Renderer.cpp)
    target_link_libraries(hotel_core PUBLIC GLESv3 log)

    set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
    add_library(native_app_glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)
    target_include_directories(native_app_glue PUBLIC ${APP_GLUE_DIR})

    add_library(hotelgame SHARED
        src/platform/android/main.cpp
        src/platform/android/AudioOutput.cpp)
    target_link_libraries(hotelgame PRIVATE hotel_core native_app_glue android EGL aaudio log)
    set_target_properties(hotelgame PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")
endif()

if(HOTEL_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_executable(test_all tests/test_all.cpp tests/RendererStub.cpp)
    target_link_libraries(test_all PRIVATE hotel_core)
    target_compile_definitions(test_all PRIVATE TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
    add_test(NAME test_all COMMAND test_all)
endif()
