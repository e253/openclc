#include "openclc_rt.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CL_CHECK(err)                                                                                                      \
    if (err != CL_SUCCESS) {                                                                                               \
        fprintf(stderr, "OpenCL Error Code %d: '%s' encountered at %s:%d\n", err, opencl_errstr(err), __FILE__, __LINE__); \
        crash();                                                                                                           \
        return 1;                                                                                                          \
    }

static cl_device_id dev;
static cl_context ctx;
static cl_command_queue queue;
static bool cl_initialized = false;

cl_context get_context() { return ctx; }
cl_device_id get_device() { return dev; }
cl_command_queue get_queue() { return queue; }

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

/// Process exits if OCLC_CRASH_ON_ERROR is defined
static void crash()
{
#ifdef OCLC_CRASH_ON_ERROR
    exit(1);
#endif
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
        crash();
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

            if (strstr(deviceClVersion, "3.") != NULL) {
                // We found a GPU that is OpenCL 3.0 capable
                dev = devices[j];
                free(devices);
                free(platforms);
                return false;
            }
        }

        free(devices);
    }

    free(platforms);

    return true;
}

int oclcInit()
{
    int _err = get_first_gpu();
    if (_err != 0) {
        crash();
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

oclc_mem oclcMalloc(size_t sz)
{
    if (sz == 0) {
        crash();
        return MEM_FAILURE;
    }

    cl_int err;
    cl_mem mem = clCreateBuffer(ctx, CL_MEM_READ_WRITE, sz, NULL, &err);

    if (err != CL_SUCCESS) {
        crash();
        // TODO: set some state for an error polling function.
        return MEM_FAILURE;
    }

    return mem;
}

int oclcFree(oclc_mem mem)
{
    cl_int err = clReleaseMemObject((cl_mem)mem);
    if (err != CL_SUCCESS) {
        crash();
        // TODO: set some state for an error polling function.
        return 1;
    } else {
        return 0;
    }
}

int oclcMemcpyHostToDevice(oclc_mem dst, void* src, size_t sz)
{
    if (sz == 0)
        return 0;
    if (dst == NULL || src == NULL) {
        fputs("null pointer supplied to copy", stderr);
        crash();
        return 1;
    }

    cl_int err = clEnqueueWriteBuffer(queue, dst, CL_FALSE, 0, sz, src, 0, NULL, NULL);
    CL_CHECK(err)

    return 0;
}

int oclcMemcpyDeviceToHost(void* dst, oclc_mem src, size_t sz)
{
    if (sz == 0)
        return 0;
    if (dst == NULL || src == NULL) {
        fputs("null pointer supplied to copy", stderr);
        crash();
        return 1;
    }

    cl_int err = clEnqueueReadBuffer(queue, dst, CL_FALSE, 0, sz, src, 0, NULL, NULL);
    CL_CHECK(err)

    return 0;
}

int oclcDeviceSynchronize()
{
    cl_int err = clFinish(queue);
    CL_CHECK(err);

    return 0;
}
