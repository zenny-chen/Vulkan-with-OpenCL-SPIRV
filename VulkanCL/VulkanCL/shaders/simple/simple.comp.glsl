#version 450
#if defined(GL_ARB_gpu_shader_int64)
#extension GL_ARB_gpu_shader_int64 : require
#else
#error No extension available for 64-bit integers.
#endif
layout(local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;


struct _21
{
    uint _m0;
};

layout(set = 0, binding = 0, std430) buffer _17_19
{
    uint _m0[];
} _19;

layout(set = 0, binding = 1, std430) buffer _17_20
{
    uint _m0[];
} _20;

layout(push_constant, std430) uniform _22_24
{
    _21 _m0;
} _24;

shared uint _5[32];
uvec3 _15 = gl_WorkGroupSize;

void main()
{
    if (gl_GlobalInvocationID.x < _24._m0._m0)
    {
        if (gl_LocalInvocationID.x < 32u)
        {
            _5[uint64_t(gl_LocalInvocationID.x)] = uint(gl_LocalInvocationID.x > 1u);
        }
        barrier();
        uint _59 = 2u + 1u;
        uint _66;
        uint _67;
        _66 = 4u + (3u + _59);
        _67 = 0u;
        uint _71;
        for (;;)
        {
            _71 = _5[uint64_t(_67)] + _66;
            if (_67 >= 31u)
            {
                break;
            }
            else
            {
                _66 = _71;
                _67++;
            }
        }
        uint64_t _80 = uint64_t(gl_GlobalInvocationID.x);
        _19._m0[_80] = ((_71 + 60u) + _20._m0[_80]) + _19._m0[_80];
    }
}

