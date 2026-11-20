#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable

layout(quads, fractional_odd_spacing, ccw) in;

#define PI 3.141592653589793238462643383279
#define NR_POINT_LIGHTS 2

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

struct UBO {
  mat4 model;
  mat4 projection;
};

layout(set = 0, binding = 0) readonly uniform UniformBufferObject { UBO ubo; };

layout(push_constant) uniform constants {
  mat4 view;
  vec4 view_position;
  layout(offset = 80) uint env_options;
  layout(offset = 96) uint options;
}
pc;

layout(binding = 1) uniform sampler2D tex_sampler[1];

layout(std140, binding = 5) uniform LightBuffer { Light lights[3]; }
lightData;

layout(location = 0) in vec2 in_texture_coord[];
layout(location = 1) in vec4 in_options[];

layout(location = 0) out vec2 out_texture_coord;
layout(location = 1) out vec3 out_position;
layout(location = 2) out vec3 out_view_position;
layout(location = 3) out mat3 out_TBN;
layout(location = 6) out vec3 out_normal;

// CSM outputs for the fragment shader
layout(location = 7) out float out_depth;
layout(location = 8) out vec4 out_cascade_coord;
layout(location = 12) out vec3 out_blend;

// Precomputed Gerstner wave constants for waves_count = 8:
// xy = normalized direction D, z = sin(phase), w = cos(phase)
const vec4 WAVE_DIR_PHASE[8] =
    vec4[8](vec4(-0.07067847, 0.99749916, -0.43269396, 0.90154087),
            vec4(0.00865757, 0.99996252, 0.10865456, -0.99407957),
            vec4(-0.17257926, 0.98499563, -0.92266263, -0.38560818),
            vec4(0.14563398, 0.98933854, 0.80778791, -0.58947324),
            vec4(-0.08826368, 0.99609715, -0.53054715, 0.84765543),
            vec4(0.13019822, 0.99148798, 0.73766451, -0.67516744),
            vec4(0.12352328, 0.99234167, 0.70492578, -0.70928108),
            vec4(0.12687266, 0.99191901, 0.72154998, -0.69236235));

// x = w (frequency), y = A (amplitude), z = WA (w * A), w = steepness
const vec4 WAVE_PARAMS[8] =
    vec4[8](vec4(1.0000000, 0.02000000, 0.02000000, 0.40000000),
            vec4(1.5000000, 0.01000000, 0.01500000, 0.26400000),
            vec4(2.2500000, 0.00500000, 0.01125000, 0.17424000),
            vec4(3.3750000, 0.00250000, 0.00843750, 0.11499840),
            vec4(5.0625000, 0.00125000, 0.00632813, 0.07589894),
            vec4(7.5937500, 0.00062500, 0.00474609, 0.05009330),
            vec4(11.3906250, 0.00031250, 0.00355957, 0.03306158),
            vec4(17.0859375, 0.00015625, 0.00266968, 0.02182064));

void main() {
  float u = gl_TessCoord.x;
  float v = gl_TessCoord.y;

  vec2 t00 = in_texture_coord[0];
  vec2 t01 = in_texture_coord[1];
  vec2 t10 = in_texture_coord[2];
  vec2 t11 = in_texture_coord[3];

  vec2 t0 = (t01 - t00) * u + t00;
  vec2 t1 = (t11 - t10) * u + t10;
  vec2 texCoord = (t1 - t0) * v + t0;

  vec4 p00 = gl_in[0].gl_Position;
  vec4 p01 = gl_in[1].gl_Position;
  vec4 p10 = gl_in[2].gl_Position;
  vec4 p11 = gl_in[3].gl_Position;

  vec4 p0 = (p01 - p00) * u + p00;
  vec4 p1 = (p11 - p10) * u + p10;
  vec4 p = (p1 - p0) * v + p0;

  const float phi_t =
      10.0f * uintBitsToFloat(pc.options); // S * (2.0 / L) * t = 10.0 * 1.0 * t

  vec3 bi = vec3(0.0);
  vec3 ta = vec3(0.0);
  vec3 n = vec3(0.0);
  vec4 displacement = vec4(0.0, 0.0, 0.0, 1.0);

  const int waves_count = 8;
  for (int i = 0; i < waves_count; i++) {
    vec2 D = WAVE_DIR_PHASE[i].xy;
    float sinPhase = WAVE_DIR_PHASE[i].z;
    float cosPhase = WAVE_DIR_PHASE[i].w;

    float w = WAVE_PARAMS[i].x;
    float A = WAVE_PARAMS[i].y;
    float WA = WAVE_PARAMS[i].z;
    float steepness = WAVE_PARAMS[i].w;

    float base = w * dot(D, p.xz) + phi_t;
    float S = sin(base);
    float C = cos(base);

    // Angle addition identities avoid 2 extra sin/cos calls per wave:
    // sin(base + phase) = S * cosPhase + C * sinPhase
    // cos(base + phase) = C * cosPhase - S * sinPhase
    float sinX = S * cosPhase + C * sinPhase;
    float cosX = C * cosPhase - S * sinPhase;

    float WAS = WA * S;
    float WAC = WA * C;
    float DxWAC = D.x * WAC;
    float DyWAC = D.y * WAC;
    float steepnessWAS = steepness * WAS;

    // bitangent
    bi.x += steepnessWAS * (D.x * D.x);
    bi.z += steepnessWAS * (D.x * D.y);
    bi.y += DxWAC;

    // tangent
    ta.x += steepnessWAS * (D.x * D.y);
    ta.z += steepnessWAS * (D.y * D.y);
    ta.y += DyWAC;

    // normal
    n.x += DxWAC;
    n.z += DyWAC;
    n.y += steepnessWAS;

    float steepnessA_cosX = (steepness * A) * cosX;
    displacement.x += steepnessA_cosX * D.x;
    displacement.z += steepnessA_cosX * D.y;
    displacement.y += A * sinX;
  }

  p.y += 2.0f + displacement.y;

  // Modify them based on the Gerstner derivatives
  vec3 binormal = normalize(vec3(1.0 - bi.x, bi.y, -bi.z));
  vec3 tangent = normalize(vec3(-ta.x, ta.y, 1.0 - ta.z));

  // Transformed into world space
  out_normal = normalize(cross(tangent, binormal));
  out_TBN = mat3(tangent, binormal, out_normal);

  vec4 world_pos = ubo.model * vec4(p.xyz, 1.0);
  out_position = world_pos.xyz;
  out_texture_coord = texCoord;
  out_view_position = vec3(pc.view_position) - out_position;

  vec4 view_pos = pc.view * world_pos;
  out_depth = -view_pos.z;

  out_cascade_coord = lightData.lights[0].cascade0 * world_pos;

  // View-space depth is out_depth = -view_pos.z
  vec4 min_splits = lightData.lights[0].cascade_min_splits;
  vec4 max_splits = lightData.lights[0].cascade_max_splits;
  vec3 inv_z_dist = 1.0 / (max_splits.xyz - min_splits.yzw);
  out_blend = (vec3(out_depth) - min_splits.yzw) * inv_z_dist;

  gl_Position = ubo.projection * view_pos;
}
