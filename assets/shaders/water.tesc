#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable

layout(vertices=4) out;

struct UBO
{
  mat4 model;
  mat4 projection;
};

layout(set = 0, binding = 0) readonly uniform UniformBufferObject {
  UBO ubo;
};

layout(push_constant) uniform constants
{
  mat4 view;
  vec4 view_position;
} pc;

layout(location = 0) in vec2 in_texture_coord[];
layout(location = 1) in vec4 in_options[];
layout(location = 2) in mat4 in_inverse_camera[];

layout(location = 0) patch out vec2 out_texture_coord;
layout(location = 1) patch out vec4 out_options;
layout(location = 2) patch out mat4 out_inverse_camera;

void main()
{
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  out_texture_coord = in_texture_coord[gl_InvocationID];
  out_options = in_options[gl_InvocationID];
  out_inverse_camera = in_inverse_camera[gl_InvocationID];
  
  if(gl_InvocationID == 0)
  {
    const int MIN_TESS_LEVEL = 4;
    const int MAX_TESS_LEVEL = 2048;
    const float MIN_DISTANCE = 20;
    const float MAX_DISTANCE = 800;

    vec4 eyeSpacePos00 = pc.view * ubo.model * gl_in[0].gl_Position;
    vec4 eyeSpacePos01 = pc.view * ubo.model * gl_in[1].gl_Position;
    vec4 eyeSpacePos10 = pc.view * ubo.model * gl_in[2].gl_Position;
    vec4 eyeSpacePos11 = pc.view * ubo.model * gl_in[3].gl_Position;

    float distance00 = clamp( (abs(eyeSpacePos00.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );
    float distance01 = clamp( (abs(eyeSpacePos01.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );
    float distance10 = clamp( (abs(eyeSpacePos10.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );
    float distance11 = clamp( (abs(eyeSpacePos11.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );

    float tessLevel0 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance10, distance00) );
    float tessLevel1 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance00, distance01) );
    float tessLevel2 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance01, distance11) );
    float tessLevel3 = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance11, distance10) );

    gl_TessLevelOuter[0] = tessLevel0;
    gl_TessLevelOuter[1] = tessLevel1;
    gl_TessLevelOuter[2] = tessLevel2;
    gl_TessLevelOuter[3] = tessLevel3;

    gl_TessLevelInner[0] = max(tessLevel1, tessLevel3);
    gl_TessLevelInner[1] = max(tessLevel0, tessLevel2);
  }
}

