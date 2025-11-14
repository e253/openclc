# OpenCL Compiler
`openclc` is an experimental compiler for opencl compute kernels that supports

1. Colocation of OpenCL kernel source code with c/cpp source code.
2. Build time compilation of kernels to SPIR-V (or PTX/Metal IR in the future)
3. Automatic device discovery
4. CUDA kernel invocation syntax

These features make for far easier use of data parallel hardware shipped with most devices.

# Roadmap

- **Vulkan Target**: GLCompute SPIR-V for Vulkan. Code Generation passes already exist in [clspv](https://github.com/google/clspv). This exchanges some OpenCL features for increased portability.
- **Metal Target**: SPIR-V Cross can reflect GLCompute SPIR-V up to MSL source which can be used in the Metal API. This sounds brittle but is the crux of WebGPU shaders running correctly on MacOS.
- **CUDA**: OpenCL kernels can be compiled to PTX with the LLVM PTX backend. It should also be possible to support many CUDA built-ins, synchronization primitives, and inline PTX.


# Usage
`vec_add.cl`
```c
#include <assert.h>
#include <openclc_rt.h>
#include <stdio.h>

kernel void add(constant float *A, constant float *B, global float *C)
{
    size_t gid = get_global_id(0);
    C[gid] = A[gid] + B[gid];
}

int main()
{
    oclcInit();

    int n = 256;
    size_t sz = n * sizeof(float);

    // Allocate Host Memory
    float* A = (float*)malloc(sz);
    float* B = (float*)malloc(sz);
    float* C = (float*)malloc(sz);

    // Initialize Host Memory
    for (int i = 0; i < n; i++) {
        A[i] = i;
        B[i] = i;
        C[i] = 0;
    }

    // Allocate Device Memory
    float* dA = (float*)oclcMalloc(sz);
    float* dB = (float*)oclcMalloc(sz);
    float* dC = (float*)oclcMalloc(sz);

    // Copy Host Values to the Device Memory
    oclcMemcpy(dA, A, sz, oclcMemcpyHostToDevice);
    oclcMemcpy(dB, B, sz, oclcMemcpyHostToDevice);
    oclcMemcpy(dC, C, sz, oclcMemcpyHostToDevice);

    dim3 gridDim = { n / 32 };
    dim3 blockDim = { 32 };

    // Invoke the Kernel!
    add<<<gridDim, blockDim>>>(dA, dB, dC);

    oclcMemcpy(C, dC, sz, oclcMemcpyDeviceToHost);

    oclcDeviceSynchronize();

    for (int i = 0; i < n; i++) {
        assert(C[i] == 2 * i);
    }

    puts("\n---------------------------\n");
    puts("Passed");
    puts("\n---------------------------\n");

    oclcFree(dA);
    oclcFree(dB);
    oclcFree(dC);
}
```
```sh
openclc vec_add.cl -o vadd
```


# Installation

`install.sh` will download the lastest release for your os/arch and extract the contents to `$HOME/.openclc`.
```sh
curl -fsSL https://raw.githubusercontent.com/e253/openclc/main/install.sh | bash
```

Alternatively, download the release archive and extract it anywhere you'd like.

OpenCLC does not need to be installed in any specific location.


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

# Credits
1. Khronos Group, for the `SPIRV-Tools`, `SPIRV-LLVM-Translator` dependencies
2. Google for the `clspv` project. I couldn't have figured out the clang frontend calls otherwise.
