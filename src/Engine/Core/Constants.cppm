export module Engine.Core.Constants;

import std;

namespace Vyroda
{
  //default textures const, needs a real assets management (unique id etc.)
  export inline constexpr std::string VYR_EMPTY{ "_vyr_empty" };
  export inline constexpr std::string VYR_ERROR{ "_vyr_error" };
  export inline constexpr std::string VYR_SAND{ "sand" };
  export inline constexpr std::string VYR_GRASS{ "grass" };
  export inline constexpr std::string VYR_GROUND{ "ground" };
  export inline constexpr std::string VYR_LOW_NOISE{ "low_noise" };
  export inline constexpr std::string VYR_HI_NOISE { "hi_noise" };
  export inline constexpr std::string VYR_SNOW{ "snow" };
  export inline constexpr std::string VYR_WATER_NORMAL_1{ "_water_normal" };
  export inline constexpr std::string VYR_WATER_NORMAL_2{ "_water_normal2" };

  export enum VYR_ENV_OPTIONS : std::uint32_t {
    HAS_FOG = 1u << 0,
    HAS_IRRADIANCE = 1u << 1
  };

  export enum VYR_MESH_OPTIONS : std::uint32_t {
    HAS_BASE_COLOR = 1u << 0,
    HAS_SPECULAR = 1u << 1,
    HAS_NORMAL = 1u << 2,
    HAS_ALPHA = 1u << 3,
    HAS_MR = 1u << 4,
    HAS_EMISSIVE = 1u << 5,
    HAS_AO = 1u << 6,
    HAS_TRANSMISSION = 1u << 7
  };

  export enum class TextureWrapMode {
    WRAP,
    CLAMP_TO_EDGE,
    MIRROR_REPEAT
  };

  export enum class AnimInterpolation {
    STEP,
    LINEAR,
    SPHERICAL_LINEAR,
    CUBIC_SPLINE
  };

  export enum class SocketStatus {
    NOT_CONNECTED,
    CONNECTED
  };

  export enum class ServerStatus {
    NOT_RUNNING,
    RUNNING
  };
}
