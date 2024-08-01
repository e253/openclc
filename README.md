# OpenCLC
This repo holds an experimental compiler for compiling OpenCL-C code to SPIR-V for use in opencl `createProgamWithIL`. It packages multiple pre-existing project into a single binary without any system dependencies at runtime.

# Installation

`install.sh` will download the lastest release for your os/arch and extract the contents to `$HOME/.openclc`.
```sh
curl -fsSL https://raw.githubusercontent.com/e253/openclc/main/install.sh | bash
```

Alternatively, download the release archive and extract it anywhere you'd like.

OpenCLC does not need to be installed in any specific location.

# Credits
1. Khronos Group, for the `SPIRV-Tools`, `SPIRV-LLVM-Translator` dependencies
2. Google for the `CLSPV` project, that works very similarly to this project, but instead targets a vulkan runtime. I couldn't have figured out the clang frontend calls otherwise.

# Improvements on Convential Wisdom.

```bash
clang -target spirv64-unknown-unknown -c -emit-llvm example.cl -o example.bc
llvm-spirv example.bc -o example.spv.debug
spirv-opt example.spv.debug -o example.spv
```

```sh
openclc example.cl -o example.spv
# `example.spv` is optimized with spirv-opt already
```

Building `llvm-spirv` using system `clang/llvm` provides an executable tailored to the system. I prefer the `zig` approach of statically linking all dependencies.

`ldd` on the `llvm-spirv` binary and `libLLVM.so`.

```sh
>> ldd /usr/local/bin/llvm-spirv
        linux-vdso.so.1 (0x00007ffcd79a8000)
        libLLVM.so.19.0 => not found
        libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f9b15b9c000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f9b15973000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f9b16571000)
        libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f9b1588c000)
        libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f9b1586c000)
>> ldd /usr/lib/llvm-18/lib/libLLVM.so 
        linux-vdso.so.1 (0x00007ffeb89b3000)
        libffi.so.8 => /lib/x86_64-linux-gnu/libffi.so.8 (0x00007f7ea1f89000)
        libedit.so.2 => /lib/x86_64-linux-gnu/libedit.so.2 (0x00007f7ea1f4f000)
        libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6 (0x00007f7ea1e68000)
        libz.so.1 => /lib/x86_64-linux-gnu/libz.so.1 (0x00007f7ea1e4c000)
        libzstd.so.1 => /lib/x86_64-linux-gnu/libzstd.so.1 (0x00007f7ea1d7d000)
        libtinfo.so.6 => /lib/x86_64-linux-gnu/libtinfo.so.6 (0x00007f7ea1d49000)
        libxml2.so.2 => /lib/x86_64-linux-gnu/libxml2.so.2 (0x00007f7ea1b67000)
        libstdc++.so.6 => /lib/x86_64-linux-gnu/libstdc++.so.6 (0x00007f7ea193b000)
        libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1 (0x00007f7ea191b000)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x00007f7ea16f2000)
        /lib64/ld-linux-x86-64.so.2 (0x00007f7ea95da000)
        libbsd.so.0 => /lib/x86_64-linux-gnu/libbsd.so.0 (0x00007f7ea16da000)
        libicuuc.so.70 => /lib/x86_64-linux-gnu/libicuuc.so.70 (0x00007f7ea14dd000)
        liblzma.so.5 => /lib/x86_64-linux-gnu/liblzma.so.5 (0x00007f7ea14b2000)
        libmd.so.0 => /lib/x86_64-linux-gnu/libmd.so.0 (0x00007f7ea14a5000)
        libicudata.so.70 => /lib/x86_64-linux-gnu/libicudata.so.70 (0x00007f7e9f887000)
```

```sh
>> ldd ./out/x86_64-linux-musl-x86_64/bin/openclc
        not a dynamic executable
>> ldd zig
        not a dynamic executable
```

All required libraries are a part of the executable!

 

# Dependencies
- [zig-bootstrap](https://github.com/ziglang/zig-bootstrap/tree/ec2dca85a340f134d2fcfdc9007e91f9abed6996) : Rev `ec2dca85a340f134d2fcfdc9007e91f9abed6996`
- [SPIRV-LLVM-Translator](https://github.com/KhronosGroup/SPIRV-LLVM-Translator/releases/tag/v18.1.2) : Release `v18.1.2`
- [SPIRV-Headers](https://github.com/KhronosGroup/SPIRV-Headers/tree/4f7b471f1a66b6d06462cd4ba57628cc0cd087d7) : Rev `4f7b471f1a66b6d06462cd4ba57628cc0cd087d7`
- [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools/releases/tag/v2024.2) : Release `v2024.2`
- [fmt](https://github.com/fmtlib/fmt)
- LLVM-18 (Build Dependency) Use `llvm.sh` to install it with `apt`. Any 18.x *should* work.

# Build

`Openclc` only builds on Debian, but can cross compile to many targets with the help of `zig`.

1. Get LLVM 18
2. Get ZVM, then zig 0.13.0
3. `./build-deps.sh <zig-target-triple> <mcpu>`
4. `./build-openclc.sh <zig-target-triple> <mcpu>`
5. `./<zig-target-triple>-<mcpu>/bin/openclc`
