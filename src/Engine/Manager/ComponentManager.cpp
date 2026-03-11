module Engine.Managers.ComponentManager;

import Engine.Component.Mesh;

namespace Vyroda
{
  void ComponentManager::clear()
  {
    _component_pools.clear();
  }
}
