module Engine.Core.CommandQueueLocator;

import std;

import Engine.Core.CommandQueue;

namespace Vyroda
{
  Vyroda::CommandQueue* Vyroda::CommandQueueLocator::_cmd_queue = nullptr;

  CommandQueue* CommandQueueLocator::get()
  {
    return _cmd_queue;
  }
  
  void CommandQueueLocator::init()
  {
    if (!_cmd_queue) {
      _cmd_queue = new Vyroda::CommandQueue();
    }
  }
}
