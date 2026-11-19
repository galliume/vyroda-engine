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
  export class ParallelManager
  {
  public:
    ParallelManager() = default;
    ~ParallelManager() = default;

    template <std::ranges::random_access_range R, typename Fn>
    void forEach(R&& range, Fn&& fn) const
    {
      std::for_each(
        std::execution::par_unseq,
        std::ranges::begin(range),
        std::ranges::end(range),
        std::forward<Fn>(fn));
    }

    template <typename T, typename Fn>
    void forEach(std::initializer_list<T> list, Fn&& fn) const
    {
      std::for_each(
        std::execution::par_unseq,
        list.begin(),
        list.end(),
        std::forward<Fn>(fn));
    }
  };
}
