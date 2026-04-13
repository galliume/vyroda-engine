export module Engine.Managers.ParallelManagerLocator;

import std;

import Engine.Managers.ParallelManager;

namespace Vyroda
{
  export class ParallelManagerLocator
  {
  public:
    static ParallelManager* get();
    static void init();

  private:
    static std::unique_ptr<ParallelManager> _parallel_manager;
  };
}
