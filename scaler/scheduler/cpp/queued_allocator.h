#pragma once

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <map>
#include <vector>

#include "utility/typedefs.h"

using task_ids_t   = std::vector<bytes>;
using queue_size_t = int;

// TODO: This impl is not complete
struct queued_allocator {
  std::map<bytes, task_ids_t>        _workers_to_task_ids;
  std::map<bytes, queue_size_t>      _workers_to_queue_size;
  std::map<bytes, bytes>             _task_id_to_worker;
  std::vector<std::pair<int, bytes>> _worker_queue;

  bool add_worker(bytes worker, int max_tasks) {
    if(std::ranges::find_if(_workers_to_task_ids, [worker](const auto& x) { return x.first == worker; }) !=
       _workers_to_task_ids.end()) {
      return false;
    }

    _workers_to_task_ids[worker] = {};
    _worker_queue.push_back({0, worker});
    _workers_to_queue_size[worker] = max_tasks;
    return true;
  }

  std::vector<bytes> remove_worker(bytes worker) {
    if(std::ranges::find_if(_workers_to_task_ids, [worker](const auto& x) { return x.first == worker; }) ==
       _workers_to_task_ids.end()) {
      return {};
    }

    std::erase_if(_worker_queue, [worker](const auto& x) { return x.second == worker; });

    task_ids_t task_ids = _workers_to_task_ids[worker];
    _workers_to_task_ids.erase(worker);

    for(const auto& task_id : task_ids) {
      _task_id_to_worker.erase(task_id);
    }

    return task_ids;
  }

  std::vector<bytes> get_worker_ids() {
    std::vector<bytes> res;
    for(const auto& [k, v] : _workers_to_task_ids) {
      res.push_back(k);
    }
    return res;
  }

  bytes get_worker_by_task_id(bytes task_id) { return _task_id_to_worker[task_id]; }

  void balance() { return; }

  std::optional<bytes> assign_task(bytes task_id) {
    if(_task_id_to_worker.contains(task_id)) {
      return _task_id_to_worker[task_id];
    }

    if(_worker_queue.empty()) return std::nullopt;

    // assert(!_worker_queue.empty());
    auto [count, worker] = _worker_queue.back();

    if(count == _workers_to_queue_size[worker]) {
      return std::nullopt;
    }

    _worker_queue.pop_back();

    _workers_to_task_ids[worker].push_back(task_id);
    _task_id_to_worker[task_id] = worker;
    _worker_queue.push_back({count + 1, worker});
    return worker;
  }

  std::optional<bytes> remove_task(bytes task_id) {
    return std::nullopt;
    // auto worker = _task_id_to_worker[task_id];

    // return {};
  }

  bytes get_assigned_worker(bytes task_id) { return _task_id_to_worker[task_id]; }

  bool has_available_worker() { return true; }

  void statistics() { return; }
};
