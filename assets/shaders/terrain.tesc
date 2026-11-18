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
  layout(offset = 80) uint env_options;
  layout(offset = 96) uint options;
} pc;

layout(location = 0) in vec2 in_texture_coord[];

layout(location = 0) out vec2 out_texture_coord[];

void main()
{
  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
  out_texture_coord[gl_InvocationID] = in_texture_coord[gl_InvocationID];

  if(gl_InvocationID == 0)
  {
    const float MIN_TESS_LEVEL = 4.0;
    const float MAX_TESS_LEVEL = 64.0;
    const float MIN_DISTANCE = 20.0;
    const float MAX_DISTANCE = 600.0;

    mat4 view = pc.view * ubo.model;
    vec4 eyeSpacePos00 = view * gl_in[0].gl_Position;
    vec4 eyeSpacePos01 = view * gl_in[1].gl_Position;
    vec4 eyeSpacePos10 = view * gl_in[2].gl_Position;
    vec4 eyeSpacePos11 = view * gl_in[3].gl_Position;

    // In view space, camera looks down -Z (points in front have z < 0, points behind have z > 0)
    bool all_behind = (eyeSpacePos00.z > 30.0) && (eyeSpacePos01.z > 30.0) && (eyeSpacePos10.z > 30.0) && (eyeSpacePos11.z > 30.0);

    vec4 clip00 = ubo.projection * eyeSpacePos00;
    vec4 clip01 = ubo.projection * eyeSpacePos01;
    vec4 clip10 = ubo.projection * eyeSpacePos10;
    vec4 clip11 = ubo.projection * eyeSpacePos11;

    const float margin_x = 40.0;
    bool all_left  = (clip00.x < -clip00.w - margin_x) && (clip01.x < -clip01.w - margin_x) && (clip10.x < -clip10.w - margin_x) && (clip11.x < -clip11.w - margin_x);
    bool all_right = (clip00.x >  clip00.w + margin_x) && (clip01.x >  clip01.w + margin_x) && (clip10.x >  clip10.w + margin_x) && (clip11.x >  clip11.w + margin_x);

    if (all_behind || all_left || all_right)
    {
      gl_TessLevelOuter[0] = 0.0;
      gl_TessLevelOuter[1] = 0.0;
      gl_TessLevelOuter[2] = 0.0;
      gl_TessLevelOuter[3] = 0.0;
      gl_TessLevelInner[0] = 0.0;
      gl_TessLevelInner[1] = 0.0;
    }
    else
    {
      const float inv_range = 1.0 / (MAX_DISTANCE - MIN_DISTANCE);
      float distance00 = clamp((abs(eyeSpacePos00.z) - MIN_DISTANCE) * inv_range, 0.0, 1.0);
      float distance01 = clamp((abs(eyeSpacePos01.z) - MIN_DISTANCE) * inv_range, 0.0, 1.0);
      float distance10 = clamp((abs(eyeSpacePos10.z) - MIN_DISTANCE) * inv_range, 0.0, 1.0);
      float distance11 = clamp((abs(eyeSpacePos11.z) - MIN_DISTANCE) * inv_range, 0.0, 1.0);

      float tessLevel0 = mix(MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance10, distance00));
      float tessLevel1 = mix(MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance00, distance01));
      float tessLevel2 = mix(MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance01, distance11));
      float tessLevel3 = mix(MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(distance11, distance10));

      gl_TessLevelOuter[0] = tessLevel0;
      gl_TessLevelOuter[1] = tessLevel1;
      gl_TessLevelOuter[2] = tessLevel2;
      gl_TessLevelOuter[3] = tessLevel3;

      gl_TessLevelInner[0] = max(tessLevel1, tessLevel3);
      gl_TessLevelInner[1] = max(tessLevel0, tessLevel2);
    }
  }
}

