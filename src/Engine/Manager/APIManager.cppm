export module Engine.Managers.APIManager;

import std;

namespace Vyroda
{
  export class APIManager
  {
  public:
    APIManager() = default;
    ~APIManager()  = default;

    void received(std::string const& message);

  private:
    void updateSkybox(std::vector<std::string> const & params);
  };
}
