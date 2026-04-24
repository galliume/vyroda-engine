export module Engine.Component.Entity;

import std;

import Engine.Core.PlpTypedef;
import Engine.Core.Logger;
import Engine.Core.GLM;
import Engine.Utils.IDHelper;

namespace Vyroda
{
  export class Entity : public std::enable_shared_from_this<Entity>
  {
  public:
    Entity();

    IDType getID() const { return _id; }
    std::string getName() const { return _name; }

    void setName(std::string const & name) { _name = name; }
    void setVisible(bool const visible = true) { _is_visible = visible; }

    bool isDirty() const { return true; }
    bool isHit(glm::vec3 point);
    bool isVisible() const { return _is_visible; }

    void setRoot(bool const is_root = true) { _is_root = is_root; }
    bool isRoot() const { return _is_root; }
    
  private:
    IDType _id { 0 };
    std::string _name {};

    bool _is_root { false };
    bool _is_visible { true };
  };
}
