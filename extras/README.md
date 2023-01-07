# Extras

This directory contains all the additional resources/libraries that JPEGView uses or sources from.

To clone the versions specified, use the following command:
`git submodule update --init --recursive`

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

libwebp library is built and included by `WEBP.dll` in `src\WEBP\lib`

License: [WebM Software Licence](https://www.webmproject.org/license/software/)

While the library source code is GPL-compatible, the full source files are not necessary to compile JPEGView.

The submodule indicates the version that the current *.lib and header files are from.

## WinGet-sylikc.JPEGView

These are the latest YAML files used to submit to the WinGet repository.

See README.txt for more information.  Will change with updates.  These files are _not_ required to compile JPEGView.
