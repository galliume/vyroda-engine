export module Engine.Core.Tools;

import std;

namespace Vyroda
{
  export class Tools
  {
  public:
      static std::vector<char> readFile(const std::string & filename);
  };
}
