#include <CL/opencl.h>
#include <stddef.h>

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

/// Get Internal Device Handle
cl_device_id get_device();

/// Get Internal Context Handle
cl_context get_context();

/// Get Internal Queue Handle
cl_command_queue get_queue();

/// Convert OpenCL error code to descriptive string
const char* opencl_errstr(cl_int err);
