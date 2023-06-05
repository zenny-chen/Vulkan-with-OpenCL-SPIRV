#ifndef let
#define let __auto_type
#endif

#ifndef NULL
#define NULL    (void*)0
#endif


kernel void BufferAddressKernel(global ulong addressBuffer[], uint elemCount)
{
    let const itemID = (uint)get_global_id(0);
    if(itemID >= elemCount) return;

    global int *pDst = (global int*)addressBuffer[0];
    global int *pSrc = (global int*)addressBuffer[1];
    if(pDst == NULL || pSrc == NULL || addressBuffer[2] != 0) {
        return;
    }

    pDst[itemID] = pSrc[itemID] + pSrc[itemID];
}

