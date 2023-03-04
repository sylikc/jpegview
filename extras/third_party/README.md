# Third-Party Submodules

This contains submodules that JPEGView uses or sources from.

The compiled binaries to these sources are included in JPEGView source, so these are used for reference, or if you would like to build from scratch.

To clone all the versions specified, use the following command:
`git submodule update --init --recursive`

_FYI: Each submodule depedency has a build script located in `extras\scripts`.  The build instructions below may possibly be outdated._

A brief description along with the licenses for each are included below.  Some have their own submodules or linked libraries.

## libheif

libheif and related libraries are built and included in `src\JPEGView\libheif\bin[64]` and `src\JPEGView\libheif\lib[64]`

The full source files are not necessary to compile JPEGView.

### dav1d

AV1 Decoder

License: [BSD 2-Clause "Simplified" License](https://code.videolan.org/videolan/dav1d/-/blob/master/COPYING)

[Compiling](https://code.videolan.org/videolan/dav1d#compile)

Initialize your environment for Win32/x64 build, then run:

1. Ensure `meson`, `ninja`, and `nasm` are on the PATH
2. mkdir build && cd build
3. meson ..
4. ninja

Find `dav1d.dll` in `build\src` directory.

### libde265

Open h.265 video codec implementation

License: [GNU Lesser General Public License v3](https://github.com/strukturag/libde265/blob/master/COPYING)

Initialize your environment for Win32/x64 build, then run

> build.bat [x86 | x64]

Find files in bin_x86 or bin_x64

### libheif

HEIF and AVIF file format decoder and encoder

License: [GNU Lesser General Public License v3](https://github.com/strukturag/libheif/blob/master/COPYING)

 md build && cd build
 cmake -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DDAV1D_LIBRARY=..\..\dav1d\build\src -DLIBDE265_LIBRARY=..\..\libde265\libde265 ..
 nmake

Find the `heif.dll` and `heif.lib` in `build\libheif`

## libjpeg-turbo

libjpeg-turbo library is built and included by JPEGView in `src\JPEGView\libjpeg-turbo`

License: 3 different BSD-style open source licenses [libjpeg-turbo license](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/LICENSE.md)

While the library source code is GPL-compatible, the full source files are not necessary to compile JPEGView.

The submodule indicates the version that the current *.lib and header files are from.

## libjxl

libjxl library is built and included by JPEGView in `src\JPEGView\libjxl`

License: [BSD 3-Clause license](https://github.com/libjxl/libjxl/blob/main/LICENSE)

To build, you'll need to make sure to do a submodule update inside the `libjxl` folder.  [Quick start guide](https://github.com/libjxl/libjxl/blob/main/README.md)

> git submodule update --init --recursive --depth 1 --recommend-shallow

While [Developing on Windows with Visual Studio 2019](https://github.com/libjxl/libjxl/blob/main/doc/developing_in_windows_vcpkg.md) uses Clang to build,
as per [qbnu's investigation](https://github.com/sylikc/jpegview/pull/99#issuecomment-1374165087), you can build with msbuild.

1. Initialize MSVS's 32-bit environment (for 64-bit builds, initialize the 64-bit environment)
2. From the libjxl folder

> mkdir build
> cd build
> cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF .. -A Win32
> msbuild.exe /p:Platform=Win32 /p:configuration="Release" LIBJXL.sln /t:jxl_dec /t:jxl_threads

For 64-bit builds, replace "Win32" with "x64" above

* `src\JPEGView\libjxl\bin[64]`
  * brotlicommon.dll, brotlidec.dll - build\third_party\brotli\Release
  * jxl_dec.dll, jxl_threads.dll - build\Release
* `src\JPEGView\libjxl\lib[64]`
  * jxl_dec.lib, jxl_threads.lib - build\lib\Release
* `src\JPEGView\libjxl\include\jxl`
  * version.h - build\lib\include\jxl
  * all other header files - extras\libjxl\lib\include\jxl

NOTE: JPEG XL requires the Visual C++ Runtime.  vcruntime140.dll and msvcp140.dll

While Microsoft allows VC++ Runtime files to be bundled directly with apps [Redistributing Visual C++ Files](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170#install-individual-redistributable-files),
because JPEGView is GPL-licensed, [Is dynamically linking my program with the Visual C++ (or Visual Basic) runtime library permitted under the GPL](https://www.gnu.org/licenses/gpl-faq.html#WindowsRuntimeAndGPL) states:
> You may not distribute these libraries in compiled DLL form with the program

Most users should already have VC++ Runtime installed, but if not, users will have to download it themselves for JPEG XL support.

## libpng-apng.src-patch

libpng library is patched with apng support and x64 support, and included by JPEGView in `src\JPEGView\libpng-apng`

The full source files are not necessary to compile JPEGView.

### libpng

License: [PNG Reference Library License version 2](http://www.libpng.org/pub/png/src/libpng-LICENSE.txt)

### libpng-apng

License: (same as libpng)

This is the patch to the official libpng library which adds animated PNG support.

### zlib

zlib is required by libpng and included by JPEGView in `src\JPEGView\libpng-apng`

License: [zlib permissive license](https://www.zlib.net/zlib_license.html)

## libwebp

libwebp library is built and included in `src\JPEGView\libwebp`

License: [WebM Software Licence](https://www.webmproject.org/license/software/)

While the library source code is GPL-compatible, the full source files are not necessary to compile JPEGView.

The submodule indicates the version that the current *.lib and header files are from.

## Little-CMS

Little-CMS 2 the color management engine used to support ICC Profiles for WebP, JPEG XL, HEIF/HEIC, AVIF

It is built and included in `src\JPEGView\lcms2`

License: [MIT License](https://github.com/mm2/Little-CMS/blob/master/COPYING)

## qoi

Quite OK Image Format

License: [MIT License](https://github.com/phoboslab/qoi/blob/master/LICENSE)

QOI source is directly compiled into JPEGView with no binary dependencies.  This submodule indicates the version that is included in JPEGView.
