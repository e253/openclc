#ifndef __OPENCLC_RT_H_
#define __OPENCLC_RT_H_

#include <CL/opencl.h>
#include <stddef.h>

/***************/
/* Runtime API */
/***************/

typedef struct {
    int x;
    int y;
    int z;
} dim3;

/// Initialize OpenCL State.
///
/// Returns 0 on success.
int oclcInit();

#define MEM_FAILURE ((void*)0)
/// Allocate `sz` bytes on the device.
///
/// Returns `MEM_FAILURE` on failure.
void* oclcMalloc(size_t sz);

/// Free device memory held by `mem`.
///
/// Returns 0 on success.
int oclcFree(void* mem);

typedef enum {
    oclcMemcpyDeviceToHost,
    oclcMemcpyHostToDevice,
} OclcMemcpyDirection;

/// Copy `sz` bytes from `src` (host) to `dst` (device)
///
/// Returns 0 on success.
int oclcMemcpy(void* dst, void* src, size_t sz, OclcMemcpyDirection dir);

/// Block execution until all previous device actions have finished
///
/// Returns 0 on success.
int oclcDeviceSynchronize();

/*************/
/* Utilities */
/*************/

#define CL_CHECK(err)                                                                                                      \
    if (err != CL_SUCCESS) {                                                                                               \
        fprintf(stderr, "OpenCL Error Code %d: '%s' encountered at %s:%d\n", err, opencl_errstr(err), __FILE__, __LINE__); \
        oclcCrash();                                                                                                       \
        return 1;                                                                                                          \
    }

/// Get Internal Device Handle
cl_device_id oclcDevice();

/// Get Internal Context Handle
cl_context oclcContext();

/// Get Internal Queue Handle
cl_command_queue oclcQueue();

/// Convert OpenCL error code to descriptive string
const char* opencl_errstr(cl_int err);

/// Crashes the program unless `OCLC_SILENT_FAIL` is defined
void oclcCrash();

/// Utility function to build spv program
///
/// Returns 0 on success.
int oclcBuildSpv(const unsigned char* spv, size_t spv_size, cl_program* prog);

/// Ensures that the gd and bd are valid before launching kernel
int oclcValidateWorkDims(dim3 gd, dim3 bd, cl_uint* work_dim);

#endif // __OPENCLC_RT_H