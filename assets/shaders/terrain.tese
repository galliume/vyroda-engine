#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable

#define NR_POINT_LIGHTS 2

layout(quads, fractional_odd_spacing, ccw) in;

struct Light {
    mat4 light_space_matrix;
    mat4 projection;
    mat4 view;
    mat4 light_space_matrix_left;
    mat4 light_space_matrix_top;
    mat4 light_space_matrix_right;
    mat4 light_space_matrix_bottom;
    mat4 light_space_matrix_back;
    mat4 cascade0;

    vec4 ads;
    vec4 clq;
    vec4 coB;
    vec4 color;
    vec4 direction;
    vec4 position;

    vec4 cascade_scale1;
    vec4 cascade_scale2;
    vec4 cascade_scale3;
    vec4 cascade_offset1;
    vec4 cascade_offset2;
    vec4 cascade_offset3;

    vec4 cascade_min_splits;
    vec4 cascade_max_splits;
    vec4 cascade_texel_sizes;
};

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

layout(binding = 1) uniform sampler2D tex_sampler[5];

layout(std430, binding = 5) readonly buffer LightBuffer {
    Light lights[];
} lightData;

layout(location = 0) in vec2 in_texture_coord[];

layout(location = 0) out vec2 out_texture_coord;
layout(location = 1) out vec4 out_weights;
layout(location = 2) out vec4 out_normal;
layout(location = 3) out vec4 out_position;
layout(location = 4) out vec3 out_view_position;

// CSM outputs for the fragment shader
layout(location = 8) out float out_depth;
layout(location = 9) out vec4 out_cascade_coord;
layout(location = 13) out vec3 out_blend;

float attenuation(float min, float max, float x)
{
  float half_range = abs(max - min) * 0.5;
  float mid = min + half_range;
  float distance = abs(x - mid);

  float att = clamp((half_range - distance) / half_range, 0.0f, 1.0f);

  return att;
}

void main()
{
  float u = gl_TessCoord.x;
  float v = gl_TessCoord.y;

  vec2 t00 = in_texture_coord[0];
  vec2 t01 = in_texture_coord[1];
  vec2 t10 = in_texture_coord[2];
  vec2 t11 = in_texture_coord[3];

  vec2 t0 = (t01 - t00) * u + t00;
  vec2 t1 = (t11 - t10) * u + t10;
  vec2 texCoord = (t1 - t0) * v + t0;
  out_texture_coord = texCoord * 500.0;

  float height = texture(tex_sampler[0], texCoord).r;

  out_weights.x = attenuation(0.0f, 0.2f, height);
  out_weights.y = attenuation(0.15f, 0.3f, height);
  out_weights.z = attenuation(0.25f, 0.9f, height);
  out_weights.w = attenuation(0.85f, 1.0f, height);

  height *= 20.0f;

  vec4 p00 = gl_in[0].gl_Position;
  vec4 p01 = gl_in[1].gl_Position;
  vec4 p10 = gl_in[2].gl_Position;
  vec4 p11 = gl_in[3].gl_Position;

  vec4 uVec = p01 - p00;
  vec4 vVec = p10 - p00;
  vec4 normal = normalize( vec4(cross(vVec.xyz, uVec.xyz), 0) );

  vec4 p0 = (p01 - p00) * u + p00;
  vec4 p1 = (p11 - p10) * u + p10;
  vec4 p = (p1 - p0) * v + p0;
  p.y -= 2.5f;
  p += normal * height;

  vec4 world_pos = ubo.model * p;
  vec4 view_pos = pc.view * world_pos;

  out_normal = normal;
  out_position = world_pos;
  out_view_position = vec3(pc.view_position - out_position);
  out_depth = -view_pos.z;
  
  out_cascade_coord = lightData.lights[0].cascade0 * world_pos;

  // View-space depth is out_depth = -view_pos.z
  vec4 min_splits = lightData.lights[0].cascade_min_splits;
  vec4 max_splits = lightData.lights[0].cascade_max_splits;
  vec3 inv_z_dist = 1.0 / (max_splits.xyz - min_splits.yzw);
  out_blend = (vec3(out_depth) - min_splits.yzw) * inv_z_dist;

  gl_Position = ubo.projection * view_pos;
}
