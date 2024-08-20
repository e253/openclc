#include <assert.h>
#include <openclc_rt.h>
#include <stdio.h>

kernel void add(constant float* A, constant float* B, global float* C)
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

    // Invoke the Kernel!
    dim3 gridDim = { n / 32 };
    dim3 blockDim = { 32 };
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
