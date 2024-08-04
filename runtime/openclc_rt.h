#include <CL/opencl.h>
#include <stddef.h>

typedef void* oclc_mem;

/// Initialize OpenCL State.
///
/// Returns 0 on success.
int oclcInit();

#define MEM_FAILURE ((void*)0)
/// Allocate `sz` bytes on the device
///
/// Returns `MEM_FAILURE` on failure.
oclc_mem oclcMalloc(size_t sz);

/// Free device memory held by `mem`.
///
/// Returns 0 on success.
int oclcFree(oclc_mem mem);

/// Copy `sz` bytes from `src` (host) to `dst` (device)
///
/// Returns 0 on success.
int oclcMemcpyHostToDevice(oclc_mem dst, void* src, size_t sz);

/// Copy `sz` bytes from `src` (device) to `dst` (host)
///
/// Returns 0 on success.
int oclcMemcpyDeviceToHost(void* dst, oclc_mem src, size_t sz);

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
