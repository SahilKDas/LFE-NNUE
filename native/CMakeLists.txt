cmake_minimum_required(VERSION 3.20)
project(LifeEngineNative LANGUAGES CXX RC)
include(CTest)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(life_engine_webview_legacy EXCLUDE_FROM_ALL WIN32 src/launcher.cpp)
add_executable(life_engine_native_core WIN32 src/main.cpp)
add_executable(life_engine WIN32 src/skia_preview.cpp src/simulation.cpp src/skia_runtime.rc)
add_custom_target(life_engine_skia_preview DEPENDS life_engine)
add_executable(life_engine_native_benchmark EXCLUDE_FROM_ALL benchmarks/simulation_benchmark.cpp src/simulation.cpp)
target_include_directories(life_engine_native_benchmark PRIVATE src)
target_compile_options(life_engine_native_benchmark PRIVATE -O3 -DNDEBUG -Wall -Wextra)
configure_file(vendor/skia/x64/libSkiaSharp.dll ${CMAKE_CURRENT_BINARY_DIR}/libSkiaSharp.dll COPYONLY)
target_include_directories(life_engine_webview_legacy PRIVATE vendor/webview2/include)
target_compile_options(life_engine_webview_legacy PRIVATE -O3 -Wall -Wextra)
target_link_options(life_engine_webview_legacy PRIVATE -municode -static -static-libgcc -static-libstdc++)
target_link_libraries(life_engine_webview_legacy PRIVATE user32 ole32 ws2_32)
add_custom_command(TARGET life_engine_webview_legacy POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/webview2/x64/WebView2Loader.dll
    $<TARGET_FILE_DIR:life_engine_webview_legacy>/WebView2Loader.dll)
target_compile_options(life_engine_native_core PRIVATE -O3 -Wall -Wextra)
target_link_options(life_engine_native_core PRIVATE -municode -static -static-libgcc -static-libstdc++)
target_link_libraries(life_engine_native_core PRIVATE user32 gdi32)
target_compile_options(life_engine PRIVATE -O3 -Wall -Wextra)
target_link_options(life_engine PRIVATE -municode -static -static-libgcc -static-libstdc++)
target_link_libraries(life_engine PRIVATE user32 gdi32)
add_custom_command(TARGET life_engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/skia/x64/libSkiaSharp.dll
    $<TARGET_FILE_DIR:life_engine>/libSkiaSharp.dll
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:life_engine>
    $<TARGET_FILE_DIR:life_engine>/life_engine_skia_preview.exe)
if(BUILD_TESTING)
  add_executable(life_engine_native_tests tests/reference_tests.cpp src/simulation.cpp)
  target_include_directories(life_engine_native_tests PRIVATE src)
  target_compile_options(life_engine_native_tests PRIVATE -O2 -Wall -Wextra)
  add_test(NAME native_reference_parity COMMAND life_engine_native_tests)
endif()
