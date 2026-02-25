module Engine.Core.ConfigManagerLocator;

import std;

namespace Vyroda
{
  std::unique_ptr<Vyroda::ConfigManager> Vyroda::ConfigManagerLocator::_config_manager;

  ConfigManager* ConfigManagerLocator::get()
  {
    return _config_manager.get();
  }

  void ConfigManagerLocator::init()
  {
    _config_manager = std::make_unique<ConfigManager>();
  }
}
