module Engine.Core.InputManagerLocator;

import std;

namespace Vyroda
{
  std::unique_ptr<Vyroda::InputManager> Vyroda::InputManagerLocator::_input_manager;

  InputManager* InputManagerLocator::get()
  {
    return _input_manager.get();
  }

  void InputManagerLocator::init(Window const * const window)
  {
    _input_manager = std::make_unique<InputManager>(window);
  }
}
