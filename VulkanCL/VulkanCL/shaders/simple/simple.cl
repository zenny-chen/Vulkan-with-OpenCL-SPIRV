#ifndef let
#define let     __auto_type
#endif

#ifndef NULL
#define NULL    (void*)0
#endif

#ifndef alignas
#define alignas(n)      _Alignas(n)
#endif

#ifndef alignof
#define alignof(expr)   _Alignof(expr)
#endif

static inline int __attribute__((overloadable)) Foo(int a)
{
    return a + 1;
}

static inline int __attribute__((overloadable)) Foo(void)
{
    return 92 + alignof(size_t);
}

alignas(16) static constant const int garray[] = { 1, 2, 3, 4 };

// Implicitly declared `layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;`
// is defined by VkSpecializationInfo.
// They are used as:
// `layout(constant_id = 0) const uint local_size_x_id = 1`;
// `layout(constant_id = 1) const uint local_size_y_id = 1`;
// `layout(constant_id = 2) const uint local_size_z_id = 1`;
// separately.
// @param pDst: layout(set = 0, binding = 0, std430) buffer
// @param pSrc: layout(set = 0, binding = 1, std430) buffer
// @param elemCount: layout(push_constant, std430) uniform
kernel void SimpleKernel(global int* restrict pDst, global int* restrict pSrc, uint elemCount)
{
    let const itemID = (uint)get_global_id(0);
    let const localID = (uint)get_local_id(0);
    
    enum { SHARED_BUFFER_LENGTH = 32 };
    
    local int sharedBuffer[SHARED_BUFFER_LENGTH];
    
    if(itemID >= elemCount) return;
    
    if(localID < (uint)SHARED_BUFFER_LENGTH) {
        sharedBuffer[localID] = localID > 1U ? 1U : 0U;
    }
    barrier(CLK_LOCAL_MEM_FENCE);
    
    int sum = 0;
    for(size_t i = 0; i < sizeof(garray) / sizeof(garray[0]); ++i) {
        sum += garray[i];
    }
    for(int i = 0; i < SHARED_BUFFER_LENGTH; ++i) {
        sum += sharedBuffer[i];
    }
    
    sum += Foo() - Foo(39);
    
    enum { flag = _Generic('c', char: -1, int: 1, default: 0) };
    
    const int constValue = sum * flag;
    
    pDst[itemID] += pSrc[itemID] + constValue;
}

