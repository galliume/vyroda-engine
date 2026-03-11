module;

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

export module Engine.Core.Json;

export namespace nlohmann
{
  using namespace ::nlohmann;
}
 
export namespace Vyroda
{
  using json = nlohmann::json;
}
