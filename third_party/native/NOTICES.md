# Native client third-party notices

SDL 3.2.28 is licensed under Zlib. RmlUi 5.1 is licensed under MIT. bgfx, bx and bimg are licensed under BSD-2-Clause. bgfx.cmake build scripts are dedicated under CC0-1.0. Apple metal-cpp headers are under Apache-2.0. SDL compiled HIDAPI and yuv2rgb notices are included below their corresponding source paths.

FreeType is used under the FreeType License (FTL), rather than the alternate GPLv2: **Portions of this software are copyright © 2024 The FreeType Project (www.freetype.org). All rights reserved.** Original FTL and alternate GPLv2 texts are retained for provenance. The license inventory also preserves the FreeType README and original BSD/X11, old MIT and zlib notices in fthash, ft-hb and the embedded gzip module; these original files retain their notices and are distributed alongside the application.

The application includes the unmodified notices in this directory. Each verified dependency archive in the project `native-sources-v1` GitHub Release retains all its original source notices, including optional components not linked into this client. Archive URLs, commits, sizes and SHA-256 hashes are locked in `data/native_dependencies.lock.json`.

Windows JPEG/PNG decoding uses the already pinned bimg source archive, with stb_image under the MIT alternative and lodepng under its Zlib license. Only JPEG/PNG parsers are enabled; AVIF, EXR, WebP and SVG decoding are excluded. The bimg base library also includes astc-encoder under Apache-2.0. These original notices and lodepng source copyright/license headers are retained under `bimg/3rdparty/`. The macOS backend retains system ImageIO. No Apple framework binaries or developer tools are redistributed. The local CMake compiler-driver cache is development tooling and is not packaged inside the application.

Company branding, original compass geometry and supplied paintings are separate project assets and receive no open-content license from these notices.
