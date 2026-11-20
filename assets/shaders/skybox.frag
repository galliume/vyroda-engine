#version 450 
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#define HAS_FOG 0 //(<< 0)

layout(location = 0) out vec4 final_color;

layout(location = 0) in vec3 tex_coord;
layout(location = 1) in vec3 view_pos;

layout(binding = 1) uniform samplerCube texSampler[];

layout(std140, push_constant) uniform constants
{
  mat4 view;
  vec4 view_position;
  layout(offset = 80) uint env_options;
  layout(offset = 96) uint options;
} pc;

vec3 uchimura_tonemap(vec3 x, float P, float a, float m, float l, float c, float b)
{
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

vec3 linear_to_hdr10(vec3 color, float white_point)
{
  const float paper_white = 203.0;
  float P = max(white_point / paper_white, 1.0);
  color = uchimura_tonemap(color, P, 1.0, 0.22, 0.4, 1.33, 0.0);

  // Convert linear Rec.709 to linear Rec.2020 (GLSL mat3 is column-major)
  const mat3 from709to2020 = mat3(
      0.6274040, 0.0690970, 0.0163916,
      0.3292820, 0.9195400, 0.0880132,
      0.0433136, 0.0113612, 0.8955950
  );
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

vec3 srgb_to_linear(vec3 color)
{ 
  float gamma        = 2.4f; // The sRGB curve for mid tones to high lights resembles a gamma of 2.4
  vec3 linear_low  = color / 12.92;
  vec3 linear_high = pow((color + 0.055) / 1.055, vec3(gamma));
  vec3 is_high     = step(0.0404482362771082, color);
  return mix(linear_low, linear_high, is_high);
}

vec3 ApplyFog(vec3 shadedColor, vec3 v)
{
  float fogDensity = 0.009;
  vec3 fogColor = vec3(0.55, 0.63, 0.72);

  float f = exp(-fogDensity * length(v));
  return (mix(fogColor, shadedColor, f));
}

void main()
{
  vec4 color = texture(texSampler[0], tex_coord);

  vec3 fog = ((pc.env_options >> HAS_FOG) & 1u) * ApplyFog(color.rgb,view_pos);
  color.rgb += fog;

  color *= 0.05;
  //@todo check how to get the precise value
  float white_point = 350;
  final_color = vec4(0.0, 0.0, 0.0, 1.0);
  final_color.rgb = linear_to_hdr10(color.rgb, white_point);
  //float exposure = 1.0;
  //final_color.rgb = vec3(1.0) - exp(-final_color.rgb * exposure);
}
