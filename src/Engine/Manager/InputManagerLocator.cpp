module Engine.Managers.InputManagerLocator;

import std;

import Engine.GUI.Window;

import Engine.Managers.InputManager;

namespace Vyroda
{
   [[clang::no_destroy]] std::unique_ptr<InputManager> InputManagerLocator::_input_manager;

  InputManager* InputManagerLocator::get()
  {
    return _input_manager.get();
  }

  void InputManagerLocator::init(Window const * const window)
  {
    _input_manager = std::make_unique<InputManager>(window);
  }
}
