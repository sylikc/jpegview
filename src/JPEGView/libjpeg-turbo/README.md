# include

Copied from the build output of extras\libjpeg-turbo:
	jconfig.h

Remaining header files copied from extras\libjpeg-turbo.

# lib

Built libraries of 32-bit variant

1. Initialize MSVS Tools 32-bit environment
2. Create a build directory
3. Generate NMake files.
   From build directory run:
   > cmake -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=Release <ABSOLUTE PATH TO extras\libjpeg-turbo>
4. Build it:
   > nmake
5. Copy *.lib files into `lib` directory.

# lib64

Built libraries of 64-bit variant

1. Initialize MSVS Tools 64-bit environment

(follow steps 2-4 above)

5. Copy *.lib files into `lib64` directory