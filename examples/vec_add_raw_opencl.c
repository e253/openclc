#define CL_TARGET_OPENCL_VERSION 300
#include <CL/opencl.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CL_CHECK(err)                                                                   \
    if (err != CL_SUCCESS) {                                                            \
        printf("OpenCL Error Code %d encountered at %s:%d\n", err, __FILE__, __LINE__); \
        exit(1);                                                                        \
    }

static cl_device_id dev;
static cl_context ctx;
static cl_program prog;

static const char* cl_source = "kernel void add(constant float *A, constant float *B, global float *C) { size_t gid = get_global_id(0); C[gid] = A[gid] + B[gid]; }";

/// Sets dev and returns true if GPU found, otherwise returns false
/// I think this is a fine heuristic for device discovery
static bool
get_first_gpu()
{
    cl_int err;

    // Get the number of platforms
    cl_uint platformCount;
    err = clGetPlatformIDs(0, NULL, &platformCount);
    CL_CHECK(err)

    // no OpenCL drivers
    if (platformCount == 0) {
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
                return true;
            }
        }

        free(devices);
    }

    free(platforms);

    return false;
}

int main()
{
    if (!get_first_gpu()) {
        return false;
    }

    cl_int err;

    ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, &err);
    CL_CHECK(err)

    prog = clCreateProgramWithSource(ctx, 1, (const char**)&cl_source, NULL, &err);
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

    cl_command_queue queue = clCreateCommandQueueWithProperties(ctx, dev, NULL, &err);
    CL_CHECK(err)

    int n = 256;
    size_t sz = n * sizeof(float);

    float* a = clSVMAlloc(ctx, CL_MEM_READ_WRITE, sz, 0);
    float* b = clSVMAlloc(ctx, CL_MEM_READ_WRITE, sz, 0);
    float* c = clSVMAlloc(ctx, CL_MEM_READ_WRITE, sz, 0);
    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MEM_READ_WRITE, a, sz, 0, NULL, NULL);
    CL_CHECK(err)
    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MEM_READ_WRITE, b, sz, 0, NULL, NULL);
    CL_CHECK(err)
    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MEM_READ_WRITE, c, sz, 0, NULL, NULL);
    CL_CHECK(err)

    for (int i = 0; i < n; i++) {
        a[i] = i;
        b[i] = i;
        c[i] = 0;
    }

    err = clEnqueueSVMUnmap(queue, a, 0, NULL, NULL);
    CL_CHECK(err)
    err = clEnqueueSVMUnmap(queue, b, 0, NULL, NULL);
    CL_CHECK(err)
    err = clEnqueueSVMUnmap(queue, c, 0, NULL, NULL);
    CL_CHECK(err)

    clFinish(queue);

    cl_kernel kernel = clCreateKernel(prog, "add", &err);
    CL_CHECK(err)

    err = clSetKernelArgSVMPointer(kernel, 0, a);
    CL_CHECK(err)
    err = clSetKernelArgSVMPointer(kernel, 1, b);
    CL_CHECK(err)
    err = clSetKernelArgSVMPointer(kernel, 2, c);
    CL_CHECK(err)

    cl_uint work_dim = 1;
    const size_t global_work_offset = 0;
    const size_t global_work_size = n;
    const size_t local_work_size = 32;
    cl_event ev;

    err = clEnqueueNDRangeKernel(queue, kernel, work_dim, &global_work_offset, &global_work_size, &local_work_size, 0, NULL, &ev);
    CL_CHECK(err)

    err = clWaitForEvents(1, &ev);
    CL_CHECK(err);
    err = clFinish(queue);
    CL_CHECK(err);

    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MEM_READ_WRITE, a, sz, 0, NULL, NULL);
    CL_CHECK(err)
    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MEM_READ_WRITE, b, sz, 0, NULL, NULL);
    CL_CHECK(err)
    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MEM_READ_WRITE, c, sz, 0, NULL, NULL);
    CL_CHECK(err)

    for (int i = 0; i < n; i++) {
        assert(c[i] == 2 * i);
    }

    puts("\n---------------------------\n");
    puts("Passed");
    puts("\n---------------------------\n");

    clSVMFree(ctx, a);
    clSVMFree(ctx, b);
    clSVMFree(ctx, c);
    clReleaseContext(ctx);
    clReleaseDevice(dev);
    clReleaseProgram(prog);
}
