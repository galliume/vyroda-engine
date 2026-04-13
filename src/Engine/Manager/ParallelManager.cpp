/**
 * @file   ParallelManager.cpp
 * @author  <Guillaume PERCEPIED>
 * @date   Mon Apr  13 19:23:49 2026
 * 
 */
module Engine.Managers.ParallelManager;

import std;

namespace Vyroda
{
  struct BarrierData
  {
    std::ptrdiff_t thread_count{ static_cast<std::ptrdiff_t>(std::thread::hardware_concurrency()) };
    std::barrier<BarrierCompletion> start{ thread_count + 1 };
    std::barrier<BarrierCompletion> done{ thread_count + 1 };
  };
  
  ParallelManager::ParallelManager()
  : _barrier_data{ std::make_unique<BarrierData>() }
  {
    _worker_contexts.resize(static_cast<std::size_t>(_thread_count));
    _workers.reserve(static_cast<std::size_t>(_thread_count));
    
    for (std::size_t t { 0 }; t < static_cast<std::size_t>(_thread_count); ++t) {
      _workers.emplace_back([this, t]() {
        while (!_shutdown.load(std::memory_order_relaxed)) {
          _barrier_data->start.arrive_and_wait();
          if (_shutdown.load(std::memory_order_relaxed)) return;
          if (_worker_contexts[t].task) {
            _worker_contexts[t].task();
          }
          _barrier_data->done.arrive_and_wait();
        }
      });
    }
  }
  
  ParallelManager::~ParallelManager()
  {
    _shutdown.store(true);
    _barrier_data->start.arrive_and_wait();
  }

  void ParallelManager::execute(std::span<std::function<void()>> tasks)
  {
    for (std::size_t i {0}; i < tasks.size(); i++) {
      _worker_contexts[i].task = std::move(tasks[i]);
    }

    _barrier_data->start.arrive_and_wait();
    _barrier_data->done.arrive_and_wait();
  }
}
