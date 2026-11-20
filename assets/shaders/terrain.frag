#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable

#define GAMMA_TRESHOLD 0.0031308
#define PI 3.141592653589793238462643383279
#define NR_POINT_LIGHTS 2

#define HEIGHT_MAP 0
#define TERRAIN_GROUND 1
#define TERRAIN_GRASS 2
#define TERRAIN_SNOW 3
#define TERRAIN_SAND 4
#define HI_NOISE 5
#define LOW_NOISE 6

#define HAS_FOG 0 //(<< 0)
#define HAS_IRRADIANCE 1

// cube maps index
#define ENVIRONMENT_MAP_INDEX 0

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

layout(push_constant) uniform constants {
  mat4 view;
  vec4 view_position;
  layout(offset = 80) uint env_options;
  layout(offset = 96) uint options;
}
pc;

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec2 in_texture_coord;
layout(location = 1) in vec4 in_weights;
layout(location = 2) in vec4 in_normal;
layout(location = 3) in vec4 in_position;
layout(location = 4) in vec3 in_view_position;

layout(location = 8) in float in_depth;
layout(location = 9) in vec4 in_cascade_coord;
layout(location = 13) in vec3 in_blend;

layout(binding = 1) uniform sampler2D tex_sampler[7];
layout(std140, binding = 5) uniform LightBuffer { Light lights[3]; }
lightData;
layout(binding = 4) uniform samplerCube env_sampler[1];
layout(binding = 6) uniform sampler2DArrayShadow csm;
layout(binding = 3) uniform samplerCubeShadow tex_shadow_sampler;

float linear_to_sRGB(float color) {
  if (color <= GAMMA_TRESHOLD) {
    return color * 12.92;
  } else {
    return pow((color * 1.055), 1 / 2.4) - 0.055;
  }
}

vec3 uchimura_tonemap(vec3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  vec3 x_pos = max(x, vec3(0.0));
  vec3 w0 = 1.0 - smoothstep(vec3(0.0), vec3(m), x_pos);
  vec3 w2 = step(vec3(m + l0), x_pos);
  vec3 w1 = 1.0 - w0 - w2;

  vec3 T = m * pow(max(x_pos, vec3(1e-6)) / m, vec3(c)) + b;
  vec3 S = P - (P - S1) * exp(CP * (x_pos - S0));
  vec3 L = m + a * (x_pos - m);

  return T * w0 + L * w1 + S * w2;
}

vec3 linear_to_hdr10(vec3 color, float white_point) {
  const float paper_white = 203.0;
  float P = max(white_point / paper_white, 1.0);
  color = uchimura_tonemap(color, P, 1.0, 0.22, 0.4, 1.33, 0.0);

  // Convert linear Rec.709 to linear Rec.2020 (GLSL mat3 is column-major)
  const mat3 from709to2020 = mat3(
      0.6274040, 0.0690970, 0.0163916,
      0.3292820, 0.9195400, 0.0880132,
      0.0433136, 0.0113612, 0.8955950);
  color = max(from709to2020 * color, vec3(0.0));

  // Scale paper-white units ([0..P]) to ST.2084 normalized nits ([0..white_point/10000])
  const float st2084_max = 10000.0;
  color *= paper_white / st2084_max;

  // Apply ST.2084 (PQ curve) for HDR10 standard
  const float m1 = 2610.0 / 4096.0 / 4.0;
  const float m2 = 2523.0 / 4096.0 * 128.0;
  const float c1 = 3424.0 / 4096.0;
  const float c2 = 2413.0 / 4096.0 * 32.0;
  const float c3 = 2392.0 / 4096.0 * 32.0;
  vec3 cp = pow(max(color, vec3(0.0)), vec3(m1));
  color = pow((c1 + c2 * cp) / (1.0 + c3 * cp), vec3(m2));

  return color;
}

vec3 srgb_to_linear(vec3 color) {
  float gamma = 2.4f; // The sRGB curve for mid tones to high lights resembles a
                      // gamma of 2.4
  vec3 linear_low = color / 12.92;
  vec3 linear_high = pow((color + 0.055) / 1.055, vec3(gamma));
  vec3 is_high = step(0.0404482362771082, color);
  return mix(linear_low, linear_high, is_high);
}

vec4 tex_color(int index) {
  vec4 color = vec4(texture(tex_sampler[index], in_texture_coord).rgb, 1.0);
  color.r = linear_to_sRGB(color.r);
  color.g = linear_to_sRGB(color.g);
  color.b = linear_to_sRGB(color.b);

  return color;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
  float P = 5.0;
  vec3 F = F0 + (max(vec3(1.0 - roughness), F0) - F0) *
                    pow(clamp(1.0 - cosTheta, 0.0, 1.0), P);

  return F;
}

float ReflectionBounce(vec3 F0) {
  return length((20.0 / 21.0) * F0 + (1.0 / 21.0));
}

float GGXDistribution(float NdH, float roughness) {
  float gamma = 2.0;

  float a = roughness * roughness;
  float a2 = a * a;
  float tmp = ((NdH * NdH) * (a2 - 1.0) + 1.0);
  float D = a2 / (PI * tmp * tmp);

  return D;
}

float SmithGeometryGGX(float roughness, float theta) {
  float a = roughness;
  float k = (a * a) / 2.0;

  return theta / (theta * (1.0 - k) + k);
}

vec3 Diffuse(vec3 diffuse, vec3 F0, float NdL, float NdV) {
  float a = 1.0 - pow(1.0 - max(NdL, 0.00001), 5);
  float b = 1.0 - pow(1.0 - max(NdV, 0.00001), 5);

  return (21.0 / (20.0 * PI)) * (1.0 - F0) * diffuse * a * b;
}

float SmoothAttenuation(vec3 l, float r_max) {
  vec2 atten_const = vec2(1.0);
  atten_const.x = 1.0 / (r_max * r_max);
  atten_const.y = 2.0 / r_max;

  float r2 = dot(l, l);
  return max(r2 * atten_const.x * (sqrt(r2) * atten_const.y - 3.0) + 1.0,
             0.0001);
}

float InverseSquareAttenuation(vec3 l) {
  float r = 20.0;
  float r_max = 100.0;
  vec2 atten_const = vec2(1.0);
  atten_const.x = (r * r) / (r_max * r_max - r * r);
  atten_const.y = (r_max * r_max);

  float r2 = dot(l, l);
  return max(atten_const.x * (atten_const.y / r2 - 1.0), 0.0001);
}

// double turbulence(double x, double y, double z, double f)
//{
//   const int W = 256;
//
//   double t = -.5;
//   for (; f < = W / 12; f *= 2)
//     t += abs(noise(x, y, z, f) / f);
//   return t;
// }
vec3 ApplyFog(vec3 shaded_color, vec3 v, float height) {
  float fog_density = 0.009;
  float height_falloff = 0.4;
  vec3 fog_color = vec3(0.4, 0.45, 0.5);

  float f = exp(-fog_density * length(v) * exp(-height_falloff * height));
  return (mix(fog_color, shaded_color, f));
}

float ShadowCalculation(vec3 light_coord) {
  vec3 p = light_coord;
  const float shadow_offset = 2.f / 512.f; //@todo push constant

  vec3 absq = abs(p);
  float mxy = max(absq.x, absq.y);
  float m = max(mxy, absq.z);

  float offset = shadow_offset * m;
  float dxy = (mxy > absq.z) ? offset : 0.0;
  float dx = (absq.x > absq.y) ? dxy : 0.0;
  vec2 oxy = vec2(offset - dx, dx);
  vec2 oyz = vec2(offset - dxy, dxy);

  vec3 limit = vec3(m, m, m);
  limit.xy -= oxy * (1.f / 512.f);
  limit.yz -= oyz * (1.f / 512.f);

  mat4 proj = lightData.lights[1].projection;
  float depth = -proj[2][2] + proj[3][2] / m;

  vec3 p1 = p;
  p1.xy -= oxy;
  p1.yz -= oyz;

  vec3 p2 = p1;
  p2.xy += oxy * 2.0;

  vec3 p3 = p2;
  p3.yz += oyz * 2.0;

  vec3 p4 = p3;
  p4.xy -= oxy * 2.0;

  float t0 = texture(tex_shadow_sampler, vec4(p, depth));
  float t1 = texture(tex_shadow_sampler, vec4(clamp(p1, -limit, limit), depth));
  float t2 = texture(tex_shadow_sampler, vec4(clamp(p2, -limit, limit), depth));
  float t3 = texture(tex_shadow_sampler, vec4(clamp(p3, -limit, limit), depth));
  float t4 = texture(tex_shadow_sampler, vec4(clamp(p4, -limit, limit), depth));

  return (t0 + t1 + t2 + t3 + t4) * 0.2;
}

float CalculateInfiniteShadow(vec3 cascade_coord0, vec3 cascade_blend) {
  vec3 cascade_scale1 = lightData.lights[0].cascade_scale1.xyz;
  vec3 cascade_scale2 = lightData.lights[0].cascade_scale2.xyz;
  vec3 cascade_scale3 = lightData.lights[0].cascade_scale3.xyz;
  vec3 cascade_offset1 = lightData.lights[0].cascade_offset1.xyz;
  vec3 cascade_offset2 = lightData.lights[0].cascade_offset2.xyz;
  vec3 cascade_offset3 = lightData.lights[0].cascade_offset3.xyz;

  vec3 p1, p2;
  vec3 cascade_coord1 = cascade_coord0 * cascade_scale1 + cascade_offset1;
  vec3 cascade_coord2 = cascade_coord0 * cascade_scale2 + cascade_offset2;
  vec3 cascade_coord3 = cascade_coord0 * cascade_scale3 + cascade_offset3;

  bool beyond_cascade2 = (cascade_blend.y >= 0.0);
  bool beyond_cascade3 = (cascade_blend.z >= 0.0);
  p1.z = float(beyond_cascade2) * 2.0;
  p2.z = float(beyond_cascade3) * 2.0 + 1.0;

  vec2 shadow_coord1 =
      (beyond_cascade2) ? cascade_coord2.xy : cascade_coord0.xy;
  vec2 shadow_coord2 =
      (beyond_cascade3) ? cascade_coord3.xy : cascade_coord1.xy;

  float depth1 = (beyond_cascade2) ? clamp(cascade_coord2.z, 0.0, 1.0)
                                   : clamp(cascade_coord0.z, 0.0, 1.0);
  float depth2 = (beyond_cascade3) ? clamp(cascade_coord3.z, 0.0, 1.0)
                                   : clamp(cascade_coord1.z, 0.0, 1.0);

  vec3 blend = clamp(cascade_blend, 0.0, 1.0);
  float weight = (beyond_cascade2) ? blend.y - blend.z : 1.0 - blend.x;

  const float texel = (3.0f / 16.0f * (1.0f / 2048.f));
  const float delta = 3.0f * texel;

  const vec4 shadow_offset[2] = vec4[2](vec4(-texel, -delta, delta, -texel),
                                        vec4(texel, delta, -delta, texel));

  float light1 = 0.0;
  if (weight > 0.0) {
    vec4 taps1;
    taps1.x =
        texture(csm, vec4(shadow_coord1 + shadow_offset[0].xy, p1.z, depth1));
    taps1.y =
        texture(csm, vec4(shadow_coord1 + shadow_offset[0].zw, p1.z, depth1));
    taps1.z =
        texture(csm, vec4(shadow_coord1 + shadow_offset[1].xy, p1.z, depth1));
    taps1.w =
        texture(csm, vec4(shadow_coord1 + shadow_offset[1].zw, p1.z, depth1));
    light1 = dot(taps1, vec4(1.0));
  }

  float light2 = 0.0;
  if (weight < 1.0) {
    vec4 taps2;
    taps2.x =
        texture(csm, vec4(shadow_coord2 + shadow_offset[0].xy, p2.z, depth2));
    taps2.y =
        texture(csm, vec4(shadow_coord2 + shadow_offset[0].zw, p2.z, depth2));
    taps2.z =
        texture(csm, vec4(shadow_coord2 + shadow_offset[1].xy, p2.z, depth2));
    taps2.w =
        texture(csm, vec4(shadow_coord2 + shadow_offset[1].zw, p2.z, depth2));
    light2 = dot(taps2, vec4(1.0));
  }

  return mix(light2, light1, weight) * 0.25;
}

void main() {
  vec4 C_diffuse = vec4(0.0);
  vec4 sand = vec4(0.0);
  vec4 ground = vec4(0.0);
  vec4 grass = vec4(0.0);
  vec4 snow = vec4(0.0);

  vec2 uv_dx = dFdx(in_texture_coord);
  vec2 uv_dy = dFdy(in_texture_coord);

  if (in_weights.y > 0.0) {
    ground = textureGrad(tex_sampler[TERRAIN_GROUND], in_texture_coord, uv_dx,
                         uv_dy) *
             in_weights.y;
  }
  if (in_weights.z > 0.0) {
    grass = textureGrad(tex_sampler[TERRAIN_GRASS], in_texture_coord, uv_dx,
                        uv_dy) *
            in_weights.z;
  }
  if (in_weights.x > 0.0) {
    sand =
        textureGrad(tex_sampler[TERRAIN_SAND], in_texture_coord, uv_dx, uv_dy) *
        in_weights.x;
  }
  if (in_weights.w > 0.0) {
    snow =
        textureGrad(tex_sampler[TERRAIN_SNOW], in_texture_coord, uv_dx, uv_dy) *
        in_weights.w;
  }

  C_diffuse += sand + ground + grass + snow;

  vec3 p = in_position.xyz;
  vec3 v = in_view_position;
  vec3 V = normalize(v);
  vec3 normal = normalize(in_normal.xyz);

  const float metallic = 0.1;
  const float roughness = 0.9; // Terrain is generally very rough

  vec3 diff_irradiance = vec3(1.0);
  vec3 spec_irradiance = vec3(0.0);

  if (bool((pc.env_options >> HAS_IRRADIANCE) & 1u)) {
    const float lod = 9.0;
    vec3 r = reflect(-V, normal);
    diff_irradiance =
        textureLod(env_sampler[ENVIRONMENT_MAP_INDEX], normal, lod).rgb;
    spec_irradiance =
        textureLod(env_sampler[ENVIRONMENT_MAP_INDEX], r, roughness * lod).rgb;
  }

  vec3 F0 = mix(vec3(0.04), C_diffuse.xyz, metallic);

  // Sun light calculation
  vec3 sun_pos = lightData.lights[0].position.xyz;
  vec3 sun_color = lightData.lights[0].color.rgb;

  vec3 L = normalize(sun_pos);
  vec3 H = normalize(L + V);
  float NdL = max(dot(normal, L), 0.0);
  float NdV = max(dot(normal, V), 0.0);
  float NdH = max(dot(normal, H), 0.0);
  float HdV = max(dot(H, V), 0.0);

  vec3 F = FresnelSchlick(HdV, F0);
  float D = GGXDistribution(NdH, roughness);
  float G1 = SmithGeometryGGX(roughness, NdV);
  float G2 = SmithGeometryGGX(roughness, NdL);
  float G = G1 * G2;

  vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

  vec3 numerator = D * G * F;
  float denominator = 4.0 * NdV * NdL + 0.0001;
  vec3 specular = numerator / denominator;

  vec3 csm_coords = in_cascade_coord.xyz / in_cascade_coord.w;
  float csm_shadow = CalculateInfiniteShadow(csm_coords, in_blend);

  vec3 out_lights = vec3(0.0);

  for (int i = 0; i < NR_POINT_LIGHTS; ++i) {

    vec3 light_pos = lightData.lights[i].position.xyz;
    vec3 l = light_pos - p;

    //@todo check thoses attenuation functions
    float attenuation = InverseSquareAttenuation(l);
    vec3 C_light = lightData.lights[i].color.rgb * attenuation;

    vec3 H = normalize(normalize(l) + V);
    float NdH = max(dot(normal, H), 0.0);
    float HdV = max(dot(H, V), 0.0);

    vec3 F = FresnelSchlick(HdV, F0);
    float D = GGXDistribution(NdH, roughness);

    vec3 numerator = D * G * F;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    vec3 curr = (kD * C_diffuse.rgb + specular) * C_light * NdL;

    //@temp
    if (NdL > 0.0 && i == 1) {
      float shadow = ShadowCalculation(-l);
      curr *= shadow;
    }

    out_lights += curr;
  }

  vec3 F_ambient = FresnelSchlickRoughness(NdV, F0, roughness);
  vec3 kS_amb = F_ambient;
  vec3 kD_amb = (1.0 - kS_amb) * (1.0 - metallic);

  vec3 C_sun = (kD * (C_diffuse.rgb / PI) + specular) * sun_color;
  C_sun *= csm_shadow;

  vec3 C_ambient = ((kD_amb * C_diffuse.xyz * diff_irradiance + specular) +
                    (kS_amb * spec_irradiance));

  vec4 color = vec4(C_ambient + C_sun + out_lights, 1.0);

  if (bool((pc.env_options >> HAS_FOG) & 1u)) {
    color.rgb = ApplyFog(color.rgb, v, p.y);
  }

  //@todo check how to get the precise value
  const float white_point = 1000.0;
  final_color = vec4(linear_to_hdr10(color.rgb, white_point), 1.0);
}
