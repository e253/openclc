#include "openclc_rt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static cl_device_id dev = NULL;
static cl_context ctx = NULL;
static cl_command_queue queue = NULL;
static bool cl_initialized = false;

cl_context oclcContext() { return ctx; }
cl_device_id oclcDevice() { return dev; }
cl_command_queue oclcQueue() { return queue; }

#define CaseReturnString(x) \
    case x:                 \
        return #x;

const char* opencl_errstr(cl_int err)
{
    switch (err) {
        CaseReturnString(CL_SUCCESS);
        CaseReturnString(CL_DEVICE_NOT_FOUND);
        CaseReturnString(CL_DEVICE_NOT_AVAILABLE);
        CaseReturnString(CL_COMPILER_NOT_AVAILABLE);
        CaseReturnString(CL_MEM_OBJECT_ALLOCATION_FAILURE);
        CaseReturnString(CL_OUT_OF_RESOURCES);
        CaseReturnString(CL_OUT_OF_HOST_MEMORY);
        CaseReturnString(CL_PROFILING_INFO_NOT_AVAILABLE);
        CaseReturnString(CL_MEM_COPY_OVERLAP);
        CaseReturnString(CL_IMAGE_FORMAT_MISMATCH);
        CaseReturnString(CL_IMAGE_FORMAT_NOT_SUPPORTED);
        CaseReturnString(CL_BUILD_PROGRAM_FAILURE);
        CaseReturnString(CL_MAP_FAILURE);
        CaseReturnString(CL_MISALIGNED_SUB_BUFFER_OFFSET);
        CaseReturnString(CL_COMPILE_PROGRAM_FAILURE);
        CaseReturnString(CL_LINKER_NOT_AVAILABLE);
        CaseReturnString(CL_LINK_PROGRAM_FAILURE);
        CaseReturnString(CL_DEVICE_PARTITION_FAILED);
        CaseReturnString(CL_KERNEL_ARG_INFO_NOT_AVAILABLE);
        CaseReturnString(CL_INVALID_VALUE);
        CaseReturnString(CL_INVALID_DEVICE_TYPE);
        CaseReturnString(CL_INVALID_PLATFORM);
        CaseReturnString(CL_INVALID_DEVICE);
        CaseReturnString(CL_INVALID_CONTEXT);
        CaseReturnString(CL_INVALID_QUEUE_PROPERTIES);
        CaseReturnString(CL_INVALID_COMMAND_QUEUE);
        CaseReturnString(CL_INVALID_HOST_PTR);
        CaseReturnString(CL_INVALID_MEM_OBJECT);
        CaseReturnString(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
        CaseReturnString(CL_INVALID_IMAGE_SIZE);
        CaseReturnString(CL_INVALID_SAMPLER);
        CaseReturnString(CL_INVALID_BINARY);
        CaseReturnString(CL_INVALID_BUILD_OPTIONS);
        CaseReturnString(CL_INVALID_PROGRAM);
        CaseReturnString(CL_INVALID_PROGRAM_EXECUTABLE);
        CaseReturnString(CL_INVALID_KERNEL_NAME);
        CaseReturnString(CL_INVALID_KERNEL_DEFINITION);
        CaseReturnString(CL_INVALID_KERNEL);
        CaseReturnString(CL_INVALID_ARG_INDEX);
        CaseReturnString(CL_INVALID_ARG_VALUE);
        CaseReturnString(CL_INVALID_ARG_SIZE);
        CaseReturnString(CL_INVALID_KERNEL_ARGS);
        CaseReturnString(CL_INVALID_WORK_DIMENSION);
        CaseReturnString(CL_INVALID_WORK_GROUP_SIZE);
        CaseReturnString(CL_INVALID_WORK_ITEM_SIZE);
        CaseReturnString(CL_INVALID_GLOBAL_OFFSET);
        CaseReturnString(CL_INVALID_EVENT_WAIT_LIST);
        CaseReturnString(CL_INVALID_EVENT);
        CaseReturnString(CL_INVALID_OPERATION);
        CaseReturnString(CL_INVALID_GL_OBJECT);
        CaseReturnString(CL_INVALID_BUFFER_SIZE);
        CaseReturnString(CL_INVALID_MIP_LEVEL);
        CaseReturnString(CL_INVALID_GLOBAL_WORK_SIZE);
        CaseReturnString(CL_INVALID_PROPERTY);
        CaseReturnString(CL_INVALID_IMAGE_DESCRIPTOR);
        CaseReturnString(CL_INVALID_COMPILER_OPTIONS);
        CaseReturnString(CL_INVALID_LINKER_OPTIONS);
        CaseReturnString(CL_INVALID_DEVICE_PARTITION_COUNT);
    default:
        return "Unknown OpenCL error code";
    }
}

/// Sets the first GPU found as the internal `dev`
static int get_first_gpu()
{
    cl_int err;

    // Get the number of platforms
    cl_uint platformCount;
    err = clGetPlatformIDs(0, NULL, &platformCount);
    CL_CHECK(err)

    // no OpenCL drivers
    if (platformCount == 0) {
        fputs("No OpenCL Drivers Found", stderr);
        oclcCrash();
        return false;
    }

    // Get the platform IDs
    cl_platform_id* platforms = (cl_platform_id*)malloc(platformCount * sizeof(cl_platform_id));
    err = clGetPlatformIDs(platformCount, platforms, NULL);
    CL_CHECK(err)

    // For each platform
    for (int i = 0; i < platformCount; i++) {
        cl_uint deviceCount;
        err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, 0, NULL, &deviceCount);
        CL_CHECK(err)

        cl_device_id* devices = (cl_device_id*)malloc(deviceCount * sizeof(cl_device_id));
        err = clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_GPU, deviceCount, devices, NULL);
        CL_CHECK(err)

        // For each device in that platform
        for (int j = 0; j < deviceCount; j++) {
            char deviceClVersion[50];
            err = clGetDeviceInfo(devices[j], CL_DEVICE_VERSION, 50, deviceClVersion, NULL);
            CL_CHECK(err)

            if (strstr(deviceClVersion, "3.") != NULL || strstr(deviceClVersion, "2.") != NULL) {
                // We found a GPU that is OpenCL 3.0 capable
                dev = devices[j];
                free(devices);
                free(platforms);
                return 0;
            }
        }

        free(devices);
    }

    fputs("No compatible GPUs found to execute Kernels", stderr);

    free(platforms);

    return 1;
}

int oclcInit()
{
    int _err = get_first_gpu();
    if (_err != 0) {
        oclcCrash();
        return 1;
    }

    cl_int err;

    ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
    CL_CHECK(err)

    queue = clCreateCommandQueueWithProperties(ctx, dev, NULL, &err);
    CL_CHECK(err)

    cl_initialized = true;

    return 0;
}

void* oclcMalloc(size_t sz)
{
    if (sz == 0) {
        oclcCrash();
        return MEM_FAILURE;
    }

    cl_int err;
    cl_mem mem = clCreateBuffer(ctx, CL_MEM_READ_WRITE, sz, NULL, &err);

    if (err != CL_SUCCESS) {
        fprintf(stderr, "OpenCL Error Code %d: '%s' encountered at %s:%d\n", err, opencl_errstr(err), __FILE__, __LINE__);
        oclcCrash();
        return MEM_FAILURE;
    }

    return (void*)mem;
}

int oclcFree(void* mem)
{
    cl_int err = clReleaseMemObject((cl_mem)mem);
    if (err != CL_SUCCESS) {
        oclcCrash();
        return 1;
    } else {
        return 0;
    }
}

int oclcMemcpy(void* dst, void* src, size_t sz, OclcMemcpyDirection dir)
{
    if (sz == 0)
        return 0;
    if (dst == NULL || src == NULL) {
        fputs("null pointer supplied to copy", stderr);
        oclcCrash();
        return 1;
    }

    switch (dir) {
    case oclcMemcpyHostToDevice: {
        cl_int err = clEnqueueWriteBuffer(queue, dst, CL_FALSE, 0, sz, src, 0, NULL, NULL);
        CL_CHECK(err)
        break;
    }
    case oclcMemcpyDeviceToHost: {
        cl_int err = clEnqueueReadBuffer(queue, src, CL_FALSE, 0, sz, dst, 0, NULL, NULL);
        CL_CHECK(err)
        break;
    }
    }

    return 0;
}

int oclcDeviceSynchronize()
{
    cl_int err = clFinish(queue);
    CL_CHECK(err);

    return 0;
}

void oclcCrash()
{
#ifndef OCLC_SILENT_FAIL
    exit(1);
#endif
}

int oclcBuildSpv(const unsigned char* spv, size_t spv_size, cl_program* prog)
{
    // TODO: Fallback to use of kernel source if SPV isn't supported

    cl_int err;
    *prog = clCreateProgramWithIL(ctx, spv, spv_size, &err);
    CL_CHECK(err);
    err = clBuildProgram(*prog, 1, &dev, NULL, NULL, NULL);

    // since we use validated SPIR-V, this is unlikely to fail.
    if (err != CL_SUCCESS) {
        size_t log_size;
        err = clGetProgramBuildInfo(*prog, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        CL_CHECK(err)
        char* log = (char*)malloc(log_size + 1);
        err = clGetProgramBuildInfo(*prog, dev, CL_PROGRAM_BUILD_LOG, log_size + 1, log, NULL);
        CL_CHECK(err)

        fprintf(stderr, "OpenCL Build Failed:\n\n%s\n", log);
        free(log);

        return 1;
    }

    return 0;
}

int oclcValidateWorkDims(dim3 gd, dim3 bd, cl_uint* work_dim)
{
    *work_dim = 3;
    if (gd.z * bd.z == 0) {
        if (gd.z != 0 || bd.z != 0) {
            fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Inconsistent work dimensions.", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
            oclcCrash();
            return 1;
        }
        *work_dim = 2;
    }
    if (gd.y * bd.y == 0) {
        if (gd.y != 0 || bd.y != 0) {
            fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Inconsistent work dimensions.", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
            oclcCrash();
            return 1;
        }
        if (gd.z != 0 || bd.z != 0) {
            fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Inconsistent work dimensions.", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
            oclcCrash();
            return 1;
        }
        *work_dim = 1;
    }
    if (gd.x * bd.x == 0) {
        fprintf(stderr, "Grid Dim (x:%d, y:%d, z:%d) incompatible with Block Dim (x:%d, y:%d, z:%d). Must have some work in the x dimension!", gd.x, gd.y, gd.z, bd.x, bd.y, bd.z);
        oclcCrash();
        return 1;
    }

    return 0;
}