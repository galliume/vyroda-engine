#version 450
#extension GL_EXT_nonuniform_qualifier : enable
// #extension GL_ARB_shader_viewport_index_layer : enable
#extension GL_NV_viewport_array2 : enable

struct UBO {
  mat4 model;
  mat4 projection;
};

layout(set = 0, binding = 0) readonly uniform UniformBufferObject { UBO ubo; };

struct Light {
  mat4 light_space_matrix[6];
  mat4 projection;
  mat4 view;
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

#define NR_POINT_LIGHTS 2

layout(std140, binding = 1) uniform LightBuffer { Light lights[3]; }
lightData;

layout(location = 2) in vec3 position;

layout(push_constant) uniform shadowMapConstants { mat4 model; }
pc;

void main() {
  int face = gl_InstanceIndex;
  gl_Layer = face;
  vec3 world = mat3(pc.model) * position + pc.model[3].xyz;
  mat4 light_vp = lightData.lights[1].light_space_matrix[face];

  gl_Position = light_vp[0] * world.x + light_vp[1] * world.y +
                light_vp[2] * world.z + light_vp[3];
}
