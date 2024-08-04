#include "openclc_rt.h"
#include "spvbin.c"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define CL_CHECK(err)                                                                                                      \
    if (err != CL_SUCCESS) {                                                                                               \
        fprintf(stderr, "OpenCL Error Code %d: '%s' encountered at %s:%d\n", err, opencl_errstr(err), __FILE__, __LINE__); \
        crash();                                                                                                           \
        return 1;                                                                                                          \
    }

static void crash()
{
#ifdef OCLC_CRASH_ON_ERROR
    exit(1);
#endif
}

static cl_program prog;
static bool prog_built = false;

static int build_spv()
{
    cl_context ctx = get_context();
    cl_device_id dev = get_device();

    cl_int err;
    prog = clCreateProgramWithIL(ctx, __spv_src, sizeof(__spv_src), &err);
    CL_CHECK(err);
    err = clBuildProgram(prog, 1, &dev, NULL, NULL, NULL);

    // since we use validated SPIR-V, this is unlikely to fail.
    if (err != CL_SUCCESS) {
        size_t log_size;
        err = clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        CL_CHECK(err)
        char* log = (char*)malloc(log_size + 1);
        err = clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, log_size + 1, log, NULL);
        CL_CHECK(err)

        fprintf(stderr, "OpenCL Build Failed:\n\n%s\n", log);
        free(log);

        return 1;
    }

    prog_built = true;
    return 0;
}

typedef struct {
    int x;
    int y;
    int z;
} dim3;

int add(dim3 gd, dim3 bd, oclc_mem A, oclc_mem B, oclc_mem C)
{
    if (!prog_built) {
        int err = build_spv();
        if (err != 0) {
            fputs("Error Building SPV\n", stderr);
            crash();
            return err;
        }
    }

    cl_int err;

    // Build Program
    cl_kernel kernel = clCreateKernel(prog, "add", &err);
    CL_CHECK(err)

    // Set Arguments
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), (cl_mem*)&A);
    CL_CHECK(err)
    err = clSetKernelArg(kernel, 1, sizeof(cl_mem), (cl_mem*)&B);
    CL_CHECK(err)
    err = clSetKernelArg(kernel, 2, sizeof(cl_mem), (cl_mem*)&C);
    CL_CHECK(err)

    cl_uint work_dim = 3;
    if (gd.z * bd.z == 0) {
        if (gd.z != 0 || bd.z != 0) {
            fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Inconsistent work dimensions.", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
            crash();
            return 1;
        }
        work_dim--;
    }
    if (gd.y * bd.y == 0) {
        if (gd.y != 0 || bd.y != 0) {
            fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Inconsistent work dimensions.", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
            crash();
            return 1;
        }
        if (gd.z != 0 || bd.z != 0) {
            fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Inconsistent work dimensions.", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
            crash();
            return 1;
        }
        work_dim--;
    }
    if (gd.x * bd.x == 0) {
        fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Must have some work in the x dimension!", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
        crash();
        return 1;
    }

    const size_t global_work_offset = 0;
    const size_t global_work_size[3] = { gd.x * bd.x, gd.y * bd.y, gd.z * bd.z };
    const size_t local_work_size[3] = { bd.x, bd.y, bd.z };

    err = clEnqueueNDRangeKernel(get_queue(), kernel, work_dim, &global_work_offset, global_work_size, local_work_size, 0, NULL, NULL);
    CL_CHECK(err)

    return 0;
}
