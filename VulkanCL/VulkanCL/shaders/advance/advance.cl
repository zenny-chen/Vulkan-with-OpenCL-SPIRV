#ifndef let
#define let __auto_type
#endif

#ifndef NULL
#define NULL    (void*)0
#endif


// Implicitly declared `layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;`
// is defined by VkSpecializationInfo.
// They are used as:
// `layout(constant_id = 0) const uint local_size_x_id = 1`;
// `layout(constant_id = 1) const uint local_size_y_id = 1`;
// `layout(constant_id = 2) const uint local_size_z_id = 1`;
// separately.
// @param pDst: layout(set = 0, binding = 0, std430) buffer
// @param pSrc: layout(set = 0, binding = 1, std430) buffer
// @param sharedBuffer: This local shared buffer is implicit and exposed as the element count of the buffer -- layout(constant_id = 3) const uint
// @param sharedBufferElemCount: layout(push_constant, std430) uniform (the first member)
// @param elemCount: layout(push_constant, std430) uniform (the second member)
kernel void AdvanceKernel(global int* restrict pDst, global int* restrict pSrc,
    local int* sharedBuffer, uint sharedBufferElemCount, uint elemCount)
{
    let const itemID = (uint)get_global_id(0) % elemCount;
    let const localID = (uint)get_local_id(0);

    if (localID < sharedBufferElemCount) {
        sharedBuffer[localID] = (int)itemID;
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    int sum = 0;
    for (uint i = 0; i < sharedBufferElemCount; ++i) {
        sum += sharedBuffer[i];
    }

    const bool flag = sub_group_all(localID < sharedBufferElemCount);
    const int constValue = flag ? sum : 0;

    atomic_add(&pDst[itemID], pSrc[itemID] + constValue);
}

