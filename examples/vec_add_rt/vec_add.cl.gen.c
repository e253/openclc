#include "openclc_rt.h"
#include "spvbin.c"
#include <stdbool.h>
#include <stdio.h>

static cl_program prog = NULL;
static bool prog_built = false;

int add(dim3 gd, dim3 bd, float* A, float* B, float* C)
{
    if (!prog_built) {
        int err = oclcBuildSpv(__spv_src, sizeof(__spv_src), &prog);
        if (err != 0) {
            return 1;
        } else {
            prog_built = true;
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

    // Validate bd, gd and set work_dim accordingly
    cl_uint work_dim;
    if (oclcValidateWorkDims(gd, bd, &work_dim) != 0) {
        return 1;
    }

    const size_t global_work_offset = 0;
    const size_t global_work_size[3] = { gd.x * bd.x, gd.y * bd.y, gd.z * bd.z };
    const size_t local_work_size[3] = { bd.x, bd.y, bd.z };

    // Launch kernel
    err = clEnqueueNDRangeKernel(oclcQueue(), kernel, work_dim, &global_work_offset, global_work_size, local_work_size, 0, NULL, NULL);
    CL_CHECK(err)

    return 0;
}
