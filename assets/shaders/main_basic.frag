#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_separate_shader_objects : enable

#define TEXTURE_COUNT 8

// env options
#define HAS_FOG 0 //(<< 0)
#define HAS_IRRADIANCE 1

// mesh options
#define HAS_BASE_COLOR 0
#define HAS_SPECULAR 1
#define HAS_NORMAL 2
#define HAS_ALPHA 3
#define HAS_MR 4
#define HAS_EMISSIVE 5
#define HAS_AO 6
#define HAS_TRANSMISSION 7

// texture map index
#define DIFFUSE_INDEX 0
#define ALPHA_INDEX 1
#define NORMAL_INDEX 2
#define SPECULAR_INDEX 3
#define METAL_ROUGHNESS_INDEX 4
#define EMISSIVE_INDEX 5
#define AO_INDEX 6
#define TRANSMISSION_INDEX 7

// depth / shadow map index
#define SHADOW_MAP_INDEX 0

// cube maps index
#define ENVIRONMENT_MAP_INDEX 0

#define SPEC_HIGHLIGHT_THRESHOLD 0.02

// constants
#define PI 3.141592653589793238462643383279
#define INV_PI 1 / PI

#define GAMMA_TRESHOLD 0.0031308
#define NR_POINT_LIGHTS 3

layout(location = 0) out vec4 final_color;

layout(location = 0) in FRAG_VAR {
  vec3 frag_pos;
  vec3 view_pos;
  vec2 texture_coord;
  vec3 norm;
  mat3 TBN;
  vec4 cascade_coord;
  vec3 blend;
}
var;

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

struct Material {
  vec4 base_color;
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec3 transmittance;
  vec3 shi_ior_diss; // shininess, ior, diss
  vec3 alpha;
  vec3 mre_factor;
  vec3 normal_translation;
  vec3 normal_scale;
  vec3 normal_rotation;
  vec3 ambient_translation;
  vec3 ambient_scale;
  vec3 ambient_rotation;
  vec3 diffuse_translation;
  vec3 diffuse_scale;
  vec3 diffuse_rotation;
  vec3 emissive_translation;
  vec3 emissive_scale;
  vec3 emissive_rotation;
  vec3 mr_translation;
  vec3 mr_scale;
  vec3 mr_rotation;
  vec3 transmission_translation;
  vec3 transmission_scale;
  vec3 transmission_rotation;
  vec3 strength; // x: normal strength, y occlusion strength
  vec4 emissive_color;
};

layout(binding = 1) uniform sampler2D tex_sampler[TEXTURE_COUNT];

layout(binding = 2) readonly buffer ObjectBuffer { Material material; };

layout(binding = 3) uniform samplerCubeShadow tex_shadow_sampler;

layout(binding = 4) uniform samplerCube env_sampler[];

layout(std430, binding = 5) readonly buffer LightBuffer { Light lights[]; }
lightData;

layout(binding = 6) uniform sampler2DArrayShadow csm;

layout(push_constant) uniform constants {
  mat4 view;
  vec4 view_position;
  layout(offset = 80) uint env_options;
  layout(offset = 96) uint options;
}
pc;

float InverseSquareAttenuation(vec3 l) {
  float r = 20.0;
  float r_max = 100.0;
  vec2 atten_const = vec2(1.0);
  atten_const.x = (r * r) / (r_max * r_max - r * r);
  atten_const.y = (r_max * r_max);

  float r2 = dot(l, l);
  return max(atten_const.x * (atten_const.y / r2 - 1.0), 0.0001);
}

float ExponentialAttenuation(vec3 l) {
  float k = 2.0;
  float r_max = 10.0;
  vec3 atten_const = vec3(1.0);
  float neg_k_square = -(k * k);
  float exp_k_neg_square = exp(neg_k_square);
  atten_const.x = neg_k_square / (r_max * r_max);
  atten_const.y = 1.0 / (1.0 - exp_k_neg_square);
  atten_const.z = exp_k_neg_square / (1.0 - exp_k_neg_square);

  float r2 = dot(l, l);
  return max(exp(r2 * atten_const.x) * atten_const.y - atten_const.z, 0.0001);
}

float SmoothAttenuation(vec3 l, float r_max) {
  vec2 atten_const = vec2(1.0);
  atten_const.x = 1.0 / (r_max * r_max);
  atten_const.y = 2.0 / r_max;

  float r2 = dot(l, l);
  return max(r2 * atten_const.x * (sqrt(r2) * atten_const.y - 3.0) + 1.0,
             0.0001);
}

float linear_to_sRGB(float color) {
  if (color <= GAMMA_TRESHOLD) {
    return color * 12.92;
  } else {
    return pow((color * 1.055), 1 / 2.4) - 0.055;
  }
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

  const float texel1 = (3.0f / 16.0f * (1.0f / 2048.f));
  const float delta1 = 3.0f * texel1;
  const vec4 shadow_offset[2] = vec4[2](vec4(-texel1, -delta1, delta1, -texel1),
                                        vec4(texel1, delta1, -delta1, texel1));

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

// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_transform/README.md
vec2 transform_uv(vec3 t, vec3 s, vec3 r, vec2 c) {
  if (t.z < 0.5)
    return c; // no transform needed

  mat3 translation = mat3(1, 0, 0, 0, 1, 0, t.x, -t.y, 1);
  float cosx = cos(r.x);
  float sinx = sin(r.x);
  mat3 rotation = mat3(cosx, sinx, 0, -sinx, cosx, 0, 0, 0, 1);
  mat3 scale = mat3(s.x, 0, 0, 0, s.y, 0, 0, 0, 1);
  mat3 matrix = mat3(1.0);
  matrix *= translation * rotation * scale;
  vec2 uvTransformed = (matrix * vec3(c.xy, 1)).xy;

  return uvTransformed;
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

float SmithGeometryGGXIBL(float roughness, float theta) {
  float a = roughness + 1;
  float k = (a * a) / 8.0;

  return theta / (theta * (1.0 - k) + k);
}

vec3 linear_to_hdr10(vec3 color, float white_point) {
  // Convert Rec.709 to Rec.2020 color space to broaden the palette
  const mat3 from709to2020 =
      mat3(0.6274040, 0.3292820, 0.0433136, 0.0690970, 0.9195400, 0.0113612,
           0.0163916, 0.0880132, 0.8955950);
  // color = from709to2020 * color;

  // Normalize HDR scene values ([0..>1] to [0..1]) for ST.2084 curve
  const float st2084_max = 10000.0;
  color *= white_point / st2084_max;

  // Apply ST.2084 (PQ curve) for HDR10 standard
  const float m1 = 2610.0 / 4096.0 / 4.0;
  const float m2 = 2523.0 / 4096.0 * 128.0;
  const float c1 = 3424.0 / 4096.0;
  const float c2 = 2413.0 / 4096.0 * 32.0;
  const float c3 = 2392.0 / 4096.0 * 32.0;
  vec3 cp = pow(abs(color), vec3(m1));
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

vec3 Diffuse(vec3 diffuse, vec3 F0, float NdL, float NdV) {
  float a = 1.0 - pow(1.0 - max(NdL, 0.0001), 5);
  float b = 1.0 - pow(1.0 - max(NdV, 0.0001), 5);

  return (21.0 / (20.0 * PI)) * (1.0 - F0) * diffuse * a * b;
}

vec3 ApplyFog(vec3 shaded_color, vec3 v, float height) {
  float fog_density = 0.09;
  float height_falloff = 0.4;
  vec3 fog_color = vec3(0.4, 0.45, 0.5);

  float f = exp(-fog_density * length(v) * exp(-height_falloff * height));
  return (mix(fog_color, shaded_color, f));
}

void main() {
  if (bool((pc.options >> HAS_ALPHA) & 1u) && material.alpha.x == 1.0) {
    vec4 alpha_color = texture(tex_sampler[ALPHA_INDEX], var.texture_coord);
    if (alpha_color.r < material.alpha.y)
      discard;
  }

  vec2 diffuse_coord =
      transform_uv(material.diffuse_translation, material.diffuse_scale,
                   material.diffuse_rotation, var.texture_coord);
  vec2 normal_coord =
      transform_uv(material.normal_translation, material.normal_scale,
                   material.normal_rotation, var.texture_coord);
  vec2 mr_coord = transform_uv(material.mr_translation, material.mr_scale,
                               material.mr_rotation, var.texture_coord);
  vec2 emissive_coord =
      transform_uv(material.emissive_translation, material.emissive_scale,
                   material.emissive_rotation, var.texture_coord);

  vec4 tex_color = vec4(1.0);
  if (bool((pc.options >> HAS_BASE_COLOR) & 1u)) {
    tex_color = texture(tex_sampler[DIFFUSE_INDEX], diffuse_coord);
  }

  vec2 raw_normal = vec2(0.0);
  if (bool((pc.options >> HAS_NORMAL) & 1u)) {
    raw_normal = texture(tex_sampler[NORMAL_INDEX], normal_coord).rg;
  }

  vec3 raw_mr = vec3(0.0, material.mre_factor.y, material.mre_factor.x);
  if (bool((pc.options >> HAS_MR) & 1u)) {
    raw_mr = texture(tex_sampler[METAL_ROUGHNESS_INDEX], mr_coord).rgb;
  }

  float ao = material.strength.y;
  if (bool((pc.options >> HAS_AO) & 1u)) {
    ao = texture(tex_sampler[AO_INDEX], var.texture_coord).r;
  }

  vec4 raw_emissive = vec4(0.0);
  if (bool((pc.options >> HAS_EMISSIVE) & 1u)) {
    raw_emissive = texture(tex_sampler[EMISSIVE_INDEX], emissive_coord);
  }

  vec3 V = normalize(var.view_pos);

  vec4 C_diffuse = material.base_color * tex_color;
  float color_alpha = C_diffuse.a;

  if (material.alpha.x == 1.0 && (color_alpha < material.alpha.y ||
                                  material.base_color.a < material.alpha.y))
    discard;

  float metallic = raw_mr.b;
  float roughness = max(raw_mr.g, 0.04);

  vec4 emissive = raw_emissive * (material.mre_factor.z * 10.0);

  vec3 diff_irradiance = vec3(0.0);
  vec3 spec_irradiance = vec3(0.0);

  vec3 normal = var.norm;
  if (bool((pc.options >> HAS_NORMAL) & 1u)) {
    vec2 xy = raw_normal * 2.0 - 1.0;
    // xy.y = -xy.y;
    float z = sqrt(max(0.0, 1.0 - dot(xy, xy)));
    normal = normalize(var.TBN * vec3(xy, z));
  }

  if (bool((pc.env_options >> HAS_IRRADIANCE) & 1u)) {
    const float lod = 9.0;
    vec3 r = reflect(-V, normal);
    diff_irradiance =
        textureLod(env_sampler[ENVIRONMENT_MAP_INDEX], normal, lod).rgb;
    spec_irradiance =
        textureLod(env_sampler[ENVIRONMENT_MAP_INDEX], r, roughness * lod).rgb;
  }

  vec3 F0 = mix(vec3(0.04), C_diffuse.xyz, metallic);

  vec3 out_lights = vec3(0.0);

  vec3 L = lightData.lights[0].direction.xyz;
  float NdL = max(dot(normal, L), 0.0);
  float NdV = max(dot(normal, V), 0.0);
  float G1_V = SmithGeometryGGX(roughness, NdV);

  vec3 C_sun = vec3(0.0);
  if (NdL > 0.0) {
    vec3 H = normalize(L + V);
    float HdV = max(dot(H, V), 0.0);
    float NdH = max(dot(normal, H), 0.0);

    vec3 F = FresnelSchlick(HdV, F0);
    float D = GGXDistribution(NdH, roughness);
    float G = G1_V * SmithGeometryGGX(roughness, NdL);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdV * NdL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    vec3 csm_coords = var.cascade_coord.xyz / var.cascade_coord.w;
    float csm_shadow = CalculateInfiniteShadow(csm_coords, var.blend);

    C_sun = (kD * (C_diffuse.rgb / PI) + specular) *
            lightData.lights[0].color.rgb * NdL * csm_shadow;
  }

  vec3 kS_amb = FresnelSchlickRoughness(NdV, F0, roughness);
  vec3 kD_amb = (1.0 - kS_amb) * (1.0 - metallic);

  for (int i = 1; i < NR_POINT_LIGHTS; ++i) {

    vec3 light_pos = lightData.lights[i].position.xyz;
    vec3 l = light_pos - var.frag_pos;

    float attenuation = InverseSquareAttenuation(l);
    vec3 C_light = lightData.lights[i].color.rgb * attenuation;

    vec3 L = normalize(l);
    vec3 H = normalize(L + V);
    float NdL = max(dot(normal, L), 0.0);
    float NdH = max(dot(normal, H), 0.0);
    float HdV = max(dot(H, V), 0.0);

    vec3 F = FresnelSchlick(HdV, F0);
    float D = GGXDistribution(NdH, roughness);
    float G = G1_V * SmithGeometryGGX(roughness, NdL);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * NdV * NdL + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    vec3 curr = (kD * (C_diffuse.rgb / PI) + specular) * C_light * NdL;

    if (NdL > 0.0 && i == 1) {
      curr *= ShadowCalculation(-l);
    }
    out_lights += curr;
  }

  vec3 C_ambient = ((kD_amb * (C_diffuse.rgb / PI) * diff_irradiance) +
                    (kS_amb * spec_irradiance)) *
                   ao * 0.01;

  vec4 color = vec4(C_ambient + C_sun + out_lights + emissive.rgb, color_alpha);

  if (bool((pc.env_options >> HAS_FOG) & 1u)) {
    color.rgb = ApplyFog(color.rgb, var.view_pos, var.frag_pos.y);
  }

  //@todo check how to get the precise value
  const float white_point = 1000.0;
  final_color = vec4(linear_to_hdr10(color.rgb, white_point), color_alpha);
}
