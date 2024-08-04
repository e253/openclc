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
int add(dim3 gd, dim3 bd, oclc_mem A, oclc_mem B, oclc_mem C);

int main()
{
    oclcInit();

    int n = 256;
    size_t sz = n * sizeof(float);

    oclc_mem dA = oclcMalloc(sz);
    oclc_mem dB = oclcMalloc(sz);
    oclc_mem dC = oclcMalloc(sz);

    float* A = (float*)malloc(sz);
    float* B = (float*)malloc(sz);
    float* C = (float*)malloc(sz);

    for (int i = 0; i < n; i++) {
        A[i] = i;
        B[i] = i;
        C[i] = 0;
    }

    oclcMemcpyHostToDevice(dA, A, sz);
    oclcMemcpyHostToDevice(dB, B, sz);
    oclcMemcpyHostToDevice(dC, C, sz);

    dim3 gridDim = { n / 8, 0, 0 };
    dim3 blockDim = { 8, 0, 0 };

    // kernel invocation
    add(gridDim, blockDim, dA, dB, dC);

    oclcMemcpyDeviceToHost(C, dC, sz);

    oclcDeviceSynchronize();

    for (int i = 0; i < n; i++) {
        assert(C[i] == 2 * i);
    }

    puts("\n---------------------------\n");
    puts("Passed\n");
    puts("---------------------------\n");

    oclcFree(dA);
    oclcFree(dB);
    oclcFree(dC);
}
