#ifndef let
#define let __auto_type
#endif

#ifndef NULL
#define NULL    (void*)0
#endif

kernel void IncKernel(global int* restrict pDst, global const int* restrict pSrc, uint elemCount)
{
    local int intBuffer[1024];
    local float floatBuffer[1024];
    
    let const itemID = (uint)get_global_id(0);
    if(itemID >= elemCount) return;
    
    let const localID = (uint)get_local_id(0);
    let const workGroupSize = (uint)get_local_size(0);
    
    intBuffer[localID] = 1;
    floatBuffer[localID] = 1.0f;
    barrier(CLK_LOCAL_MEM_FENCE);
    
    local void *pLocalMem = itemID < workGroupSize ? (local void*)intBuffer : (local void*)floatBuffer;
    
    int sum = 0;
    for(uint i = 0; i < workGroupSize; i++) {
        // This will generate `OpCapability VariablePointers`
        sum += *((local int*)pLocalMem + i);
    }
    
    pDst[itemID] = pSrc[itemID] + sum;

    if(itemID == 0) {
        pDst[itemID] = (int)workGroupSize;
    }
}

kernel void DoubleKernel(global uint *pDst, global const uint *pSrc, uint elemCount)
{
    let const itemID = (uint)get_global_id(0);
    if(itemID < 1 || itemID >= elemCount) return;
    
    pDst[itemID] = pSrc[itemID] * 2U;
    
    if(itemID == 1) {
        pDst[itemID] = (uint)get_local_size(0);
    }
}

