# Building from libwebp

These are prebuilt binaries from the libwebp at the commit of the submodule.

## 32-bit

1. Initialize MSVS Tools 32-bit environment
2. Clear the output directory `extras\libwebp\output`
3. Start in the libwebp source directory `cd /d extras\libwebp`
4. Build it: `nmake /f Makefile.vc CFG=release-static RTLIBCFG=static OBJDIR=output`
5. Copy (`libwebp.lib` and `libwebpdemux.lib`) from `extras\libwebp\output\release-static\x86\lib` to `src\WEBP\lib`

## 64-bit

1. Initialize MSVS Tools 64-bit environment

(follow steps 2-4 above)

5. Copy (`libwebp.lib` and `libwebpdemux.lib`) from `extras\libwebp\output\release-static\x86\lib` to `src\WEBP\lib`
   Rename to be `libwebp64.lib` and `libwebpdemux64.lib`

## Include files

Follow the instructions at `src\WEBP\include\README.md`
