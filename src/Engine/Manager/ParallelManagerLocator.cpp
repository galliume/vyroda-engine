module Engine.Managers.ParallelManagerLocator;

import std;

namespace Vyroda
{
  [[clang::no_destroy]] std::unique_ptr<Vyroda::ParallelManager> Vyroda::ParallelManagerLocator::_parallel_manager;

  ParallelManager* ParallelManagerLocator::get()
  {
    return _parallel_manager.get();
  }
}
