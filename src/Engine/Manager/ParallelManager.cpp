/**
 * @file   ParallelManager.cpp
 * @author  <Guillaume PERCEPIED>
 * @date   Mon Apr  13 19:23:49 2026
 *
 */
module Engine.Managers.ParallelManager;

import std;

import Engine.Core.Logger;

namespace Vyroda
{
  struct BarrierData
  {
    std::ptrdiff_t thread_count{ static_cast<std::ptrdiff_t>(std::thread::hardware_concurrency()) };
    std::barrier<> start{ thread_count + 1 };
    std::barrier<> done{ thread_count + 1 };
  };

  ParallelManager::ParallelManager()
  : _barrier_data{ std::make_unique<BarrierData>() }
  {
    _worker_contexts.resize(static_cast<std::size_t>(_thread_count));
    _workers.reserve(static_cast<std::size_t>(_thread_count));

    for (std::size_t t { 0 }; t < static_cast<std::size_t>(_thread_count); ++t) {
      _workers.emplace_back([this, t](std::stop_token stoken) {
        while (!stoken.stop_requested()) {
          _barrier_data->start.arrive_and_wait();
          if (stoken.stop_requested()) break;
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
    for(auto& worker : _workers) {
        worker.request_stop();
    }
    _barrier_data->start.arrive_and_wait();
  }

  void ParallelManager::execute(std::span<std::function<void()>> tasks)
  {
    for (std::size_t i {0}; i < tasks.size(); i++) {
      _worker_contexts[i].task = std::move(tasks[i]);
    }

    _barrier_data->start.arrive_and_wait();
    _barrier_data->done.arrive_and_wait();

    for (std::size_t i {0}; i < tasks.size(); i++) {
      _worker_contexts[i].task = nullptr;
    }
  }
}
