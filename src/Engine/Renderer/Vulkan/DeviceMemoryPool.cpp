/**
 * @file   DeviceMemoryPool.cpp
 * @author  <Guillaume PERCEPIED>
 * @date   Sun Mar  8 19:42:30 2026
 * 
 * @brief  Device memory pool
 * @todo improve buffer size management
 *
 */
module Engine.Renderer.VulkanDeviceMemoryPool;

import std;

import Engine.Core.Logger;
import Engine.Core.Volk;

namespace Vyroda
{
  DeviceMemoryPool::DeviceMemoryPool(
    VkPhysicalDeviceProperties2 device_props,
    VkPhysicalDeviceMaintenance3Properties maintenance_props,
    VkPhysicalDeviceMemoryProperties memory_props)
    : _device_props(device_props)
    , _maintenance_props(maintenance_props)
    , _memory_props(memory_props)
  {
    
    _memory_allocation_size.resize(_memory_props.memoryTypeCount);
  }

  DeviceMemory* DeviceMemoryPool::get(
    VkDevice const & device,
    VkDeviceSize const size,
    std::uint32_t const memory_type,
    VkBufferUsageFlags const usage,
    VkDeviceSize const alignment,
    DeviceBufferType const buffer_type,
    bool forceNew)
  {
    constexpr VkDeviceSize MiB { 1024ull * 1024ull };
    VkDeviceSize min_block_size { 16ull * MiB };

    switch (buffer_type) {
      case DeviceBufferType::VERTEX:
        min_block_size = 16ull * MiB;
        break;
      case DeviceBufferType::UNIFORM:
        min_block_size = 1ull * MiB;
        break;
      case DeviceBufferType::STAGING:
      default:
        min_block_size = 16ull * MiB;
        break;
    }

    VkDeviceSize const heap_size {
      _memory_props.memoryHeaps[_memory_props.memoryTypes[memory_type].heapIndex].size
    };
    VkDeviceSize const max_alloc_size {
      (_maintenance_props.maxMemoryAllocationSize > 0)
        ? std::min(heap_size, _maintenance_props.maxMemoryAllocationSize)
        : heap_size
    };
    VkDeviceSize const requested_block_size { forceNew ? size : std::max(size, min_block_size) };
    VkDeviceSize const buffer_size {
      (size > max_alloc_size) ? size : std::min(requested_block_size, max_alloc_size)
    };

    std::lock_guard<std::mutex> lock(_mutex);

    if (!forceNew) {
      auto const pool_type { _pool.find(memory_type) };
      if (pool_type != _pool.end()) {
        auto const pool_usage { pool_type->second.find(usage) };
        if (pool_usage != pool_type->second.end()) {
          for (auto& dm : pool_usage->second) {
            if (!dm->isFull() && dm->hasEnoughSpaceLeft(size, alignment)) {
              return dm.get();
            }
          }
        }
      }
    }

    if (_memory_allocation_count >= _device_props.properties.limits.maxMemoryAllocationCount) {
      Logger::error("Max number of active allocations reached");
    }

    if (_memory_allocation_size.at(memory_type) + buffer_size > heap_size) {
      Logger::error("Max size of memory allocation reached");
    }

    auto& bucket { _pool[memory_type][usage] };
    bucket.emplace_back(std::make_unique<DeviceMemory>(
      device, memory_type, buffer_size, _device_memory_count, alignment));

    _memory_allocation_count += 1;
    _memory_allocation_size.at(memory_type) += buffer_size;
    _device_memory_count += 1;
    _memory_size_allocated += buffer_size;

    return bucket.back().get();
  }

  void DeviceMemoryPool::clear()
  {
    std::lock_guard<std::mutex> lock(_mutex);

    for (auto& memory_type : _pool) {
      for (auto& usage : memory_type.second) {
        for (auto& mem : usage.second) {
          clear(*mem.get());
        }
      }
    }

    _pool.clear();

    _memory_allocation_count = 0; //should be zero anyway
    _memory_size_allocated = 0;
    _memory_allocation_size.clear();
    _memory_allocation_size.resize(_memory_props.memoryTypeCount);
    _device_memory_count = 0; //should be zero anyway
  }

  void DeviceMemoryPool::clear(DeviceMemory& deviceMemory)
  {
    auto const block_size { deviceMemory.getSize() };
    _memory_allocation_size.at(deviceMemory.getType()) -= block_size;
    _memory_size_allocated -= block_size;
    _memory_allocation_count -= 1;
    _device_memory_count -= 1;
    deviceMemory.clear();
    //Logger::debug("Clearing device memory id {}. {} left", deviceMemory->getID(), _device_memory_count);
  }
}
