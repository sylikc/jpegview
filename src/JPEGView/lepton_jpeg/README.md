# include

Build a header file (in case they change the interface, it is hande to have such a header to update the wrapper, but completely not required for building JPEGView):
``` shell
cargo install --force cbindgen
cbindgen --crate lepton_jpeg --output my_header.h
```

# lib

Build libraries of 64-bit variant
N/A

# lib64

Build libraries of 64-bit variant:
``` shell
git clone https://github.com/microsoft/lepton_jpeg_rust
cd lepton_jpeg_rust

set RUSTFLAGS=-Ccontrol-flow-guard -Ctarget-feature=+crt-static,+avx2,+lzcnt -Clink-args=/DYNAMICBASE/CETCOMPAT
cargo build --locked --release 2>&1
copy target\release\lepton_jpeg.dll target\release\lepton_jpeg_avx2.dll
copy target\release\lepton_jpeg.pdb target\release\lepton_jpeg_avx2.pdb
copy target\release\lepton_jpeg_util.exe target\release\lepton_jpeg_util_avx2.exe
copy target\release\lepton_jpeg_util.pdb target\release\lepton_jpeg_util_avx2.pdb

set RUSTFLAGS=-Ccontrol-flow-guard -Ctarget-feature=+crt-static -Clink-args=/DYNAMICBASE/CETCOMPAT
cargo build --locked --release 2>&1
```