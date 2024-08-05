#include "openclc_rt.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/// Decls to be in generated header
typedef struct {
    int x;
    int y;
    int z;
} dim3;
int add(dim3 gd, dim3 bd, float* A, float* B, float* C);

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
    add(gridDim, blockDim, dA, dB, dC);

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
