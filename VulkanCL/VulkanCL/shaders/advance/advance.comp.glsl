#version 450
#if defined(GL_ARB_gpu_shader_int64)
#extension GL_ARB_gpu_shader_int64 : require
#else
#error No extension available for 64-bit integers.
#endif
#extension GL_KHR_shader_subgroup_vote : require
layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;


struct _17
{
    uint _m0;
    uint _m1;
};

layout(constant_id = 3) const uint _21 = 1u;

layout(set = 0, binding = 0, std430) buffer _13_15
{
    uint _m0[];
} _15;

layout(set = 0, binding = 1, std430) buffer _13_16
{
    uint _m0[];
} _16;

layout(push_constant, std430) uniform _18_20
{
    _17 _m0;
} _20;

uvec3 _11 = gl_WorkGroupSize;
shared uint _24[_21];

void main()
{
    uint _38 = gl_GlobalInvocationID.x % _20._m0._m1;
    bool _42 = gl_LocalInvocationID.x < _20._m0._m0;
    if (_42)
    {
        _24[uint64_t(gl_LocalInvocationID.x)] = _38;
    }
    barrier();
    uint _58;
    uint _59;
    uint _72;
    if (_20._m0._m0 != 0u)
    {
        _58 = 0u;
        _59 = 0u;
        uint _63;
        for (;;)
        {
            _63 = _24[uint64_t(_59)] + _58;
            uint _65 = _59 + 1u;
            if (_65 >= _20._m0._m0)
            {
                break;
            }
            else
            {
                _58 = _63;
                _59 = _65;
            }
        }
        _72 = _63;
    }
    else
    {
        _72 = 0u;
    }
    uint64_t _78 = uint64_t(_38);
    uint _85 = atomicAdd(_15._m0[_78], ((subgroupAll(uint(_42)) == 0u) ? 0u : _72) + _16._m0[_78]);
}

