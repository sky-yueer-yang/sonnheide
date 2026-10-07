add_library(sonnheide_native_platform STATIC
 "${CMAKE_SOURCE_DIR}/platform/src/native_platform.cpp"
 "${CMAKE_SOURCE_DIR}/presentation/native/bgfx_render_interface.cpp")
if(APPLE)
 target_sources(sonnheide_native_platform PRIVATE "${CMAKE_SOURCE_DIR}/platform/src/native_platform.mm")
elseif(WIN32)
 target_sources(sonnheide_native_platform PRIVATE "${CMAKE_SOURCE_DIR}/platform/src/image_decode_bimg.cpp")
 target_link_libraries(sonnheide_native_platform PRIVATE bimg_decode)
endif()
target_include_directories(sonnheide_native_platform PUBLIC
 "${CMAKE_SOURCE_DIR}/platform/include" "${CMAKE_SOURCE_DIR}/presentation/native")
target_include_directories(sonnheide_native_platform PRIVATE
 "${SONNHEIDE_NATIVE_SOURCES}/bgfx/examples/common/imgui"
 "${SONNHEIDE_NATIVE_SOURCES}/bgfx/examples/common/debugdraw")
target_compile_features(sonnheide_native_platform PUBLIC cxx_std_20)
set_target_properties(sonnheide_native_platform PROPERTIES OBJCXX_STANDARD 20 CXX_EXTENSIONS OFF)
target_link_libraries(sonnheide_native_platform PUBLIC SDL3::SDL3-static RmlUi::Core bgfx bx)
if(APPLE)
 target_link_libraries(sonnheide_native_platform PRIVATE "-framework Foundation" "-framework ImageIO" "-framework CoreGraphics" "-framework Cocoa" "-framework Metal" "-framework QuartzCore")

endif()
if(BUILD_TESTING)
 add_executable(sonnheide_native_image_decode_tests
  "${CMAKE_SOURCE_DIR}/tests/native_image_decode_tests.cpp")
 if(APPLE)
  target_sources(sonnheide_native_image_decode_tests PRIVATE "${CMAKE_SOURCE_DIR}/platform/src/image_decode_bimg.cpp")
 endif()
 target_link_libraries(sonnheide_native_image_decode_tests PRIVATE sonnheide_native_platform bimg_decode)
 add_test(NAME native_image_decoding COMMAND sonnheide_native_image_decode_tests "${CMAKE_SOURCE_DIR}")
 set_tests_properties(native_image_decoding PROPERTIES TIMEOUT 60)
endif()

if(MSVC)
 target_compile_options(sonnheide_native_platform PUBLIC /utf-8)
endif()
