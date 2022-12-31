# Extras

This directory contains all the additional resources/libraries that JPEGView uses or sources from.

To close the versions specified, use the following command:
`git submodule update --init --recursive`

## libjpeg-turbo

libjpeg-turbo library is built and included by JPEGView in src\JPEGView\libjpeg-turbo

While the library source code is GPL-compatible, the full source files are not necessary to compile JPEGView.

The submodule indicates the version that the current *.lib and header files are from.

## libwebp

libwebp library is built and included by WEBP.dll in src\WEBP\lib

While the library source code is GPL-compatible, the full source files are not necessary to compile JPEGView.

The submodule indicates the version that the current *.lib and header files are from.

## libpng-apng.src-patch

libpng library is patched with apng support and x64 support, and included by JPEGView in src\JPEGView\libpng-apng

The full source files are not necessary to compile JPEGView.

### libpng

License: PNG Reference Library License version 2

### libpng-apng

License: (same as libpng)

This is the patch to the official libpng library which adds animated PNG support.

### zlib

License: zlib permissive license

zlib is required by libpng and included by JPEGView in src\JPEGView\libpng-apng

## WinGet-sylikc.JPEGView

These are the latest YAML files used to submit to the WinGet repository.

See README.txt for more information.  Will change with updates.  These files are _not_ required to compile JPEGView.
