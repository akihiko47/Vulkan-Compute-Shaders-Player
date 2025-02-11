layout (local_size_x = 16, local_size_y = 16) in;

layout (rgba16f, set = 0, binding = 0) uniform writeonly image2D outImage;

layout( push_constant ) uniform constants {
    vec4 data1;
    vec4 data2;
    vec4 data3;
    vec4 data4;
} pc;

