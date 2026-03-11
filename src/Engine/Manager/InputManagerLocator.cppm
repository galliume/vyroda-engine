export module Engine.Managers.InputManagerLocator;

import std;

import Engine.GUI.Window;

import Engine.Managers.InputManager;

namespace Vyroda
{
  export class InputManagerLocator
  {
  public:
    static InputManager* get();
    static void init(Window const * const window);

  private:
    static std::unique_ptr<InputManager> _input_manager;
  };
}
