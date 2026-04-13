/**
 * @file   ParallelManager.cppm
 * @author  <Guillaume PERCEPIED>
 * @date   Mon Apr  13 19:23:46 2026
 * 
 * @brief  parallel jobs manager 
 * 
 */
export module Engine.Managers.ParallelManager;

import std;

namespace Vyroda
{
 /**
   * @struct BarrierCompletion
   * @brief sync barrier used for multi threaded entities update
  **/
  struct BarrierCompletion
  {
    void operator()() noexcept {}
  };

  struct WorkerContext
  {
    //std::function_ref<void()> task;
    //std::move_only_function<void()> task;
    std::function<void()> task;
  };

  struct BarrierData;
  
  export class ParallelManager
  {
  public:

    ParallelManager();
    ~ParallelManager();

    std::size_t size() const { return _workers.size(); }
   
    void execute(std::span<std::function<void()>> tasks);
     
  private:
    
    //check std::execution: https://en.cppreference.com/w/cpp/experimental/execution.html
    std::ptrdiff_t _thread_count { static_cast<std::ptrdiff_t>(std::thread::hardware_concurrency()) };
    std::unique_ptr<BarrierData> _barrier_data;

    std::vector<WorkerContext> _worker_contexts;
    std::atomic<bool> _shutdown{false};
    std::vector<std::jthread> _workers;    
  };
}
