// why don't kernel pointer args default to global? ^_^

/// C = A + B
kernel void add(constant float *A, constant float *B, global float *C)
{
    size_t gid = get_global_id(0);
    C[gid] = A[gid] + B[gid];
}
