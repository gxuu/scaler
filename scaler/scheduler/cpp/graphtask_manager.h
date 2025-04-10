#pragma once

#include <capnp/common.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/array.h>
#include <kj/common.h>

#include <queue>
#include <zmq.hpp>

// #include "client_manager.h"
#include "async_binder.h"
#include "client_manager.h"
#include "common.capnp.h"
#include "message.capnp.h"
#include "task_manager.h"
#include "utility/typedefs.h"
// struct client_manager;

enum struct _NodeTaskState {
  Inactive,
  Running,
  Canceled,
  Failed,
  Success,
};

struct _TaskInfo {
  _NodeTaskState     state;
  bytes              task;
  std::vector<bytes> result_obj_ids;
};

enum struct _GraphState {
  Running,
  Cancelling,
};

struct TopologicalSorter {
  static constexpr int _NODE_OUT  = -1;
  static constexpr int _NODE_DONE = -2;

  TopologicalSorter() { printf("default ctor\n"); }

  TopologicalSorter(std::map<bytes, std::set<bytes>> graph) : _npassedout(0), _nfinished(0) {
    printf("real\n");
    for(const auto& [node, predecessors] : graph) {
      add(node, predecessors);
    }
  }

  struct nodeinfo {
    bytes              node;
    int                npred;
    std::vector<bytes> succs;

    nodeinfo() = default;
    nodeinfo(bytes node) : node(node), npred(0) {}
  };

  std::map<bytes, nodeinfo> _node2info;
  std::vector<bytes>        _ready_nodes;
  int                       _npassedout;
  int                       _nfinished;

  nodeinfo& get_nodeinfo(bytes node) {
    if(_node2info.contains(node)) {
      return _node2info[node];
    } else {
      _node2info[node] = nodeinfo(node);
      return _node2info[node];
    }
  }

  void add(bytes node, std::set<bytes> pred) {
    // TODO: Make this better
    assert(_ready_nodes.empty());

    auto& ninfo = get_nodeinfo(node);
    ninfo.npred += pred.size();
    for(const auto& p : pred) {
      auto& pinfo = get_nodeinfo(p);
      pinfo.succs.push_back(node);
    }
  }

  bool acyclic() { return true; }

  void prepare() {
    printf("prepare\n");
    assert(_ready_nodes.empty());
    for(const auto& [k, v] : _node2info)
      if(v.npred == 0)
        _ready_nodes.push_back(v.node);
      else
        printf("npred = %d\n", v.npred);
    assert(acyclic());
  }

  std::vector<bytes> get_ready() {
    assert(!_ready_nodes.empty());
    std::vector<bytes> res = _ready_nodes;
    for(const auto& node : res) {
      _node2info[node].npred = _NODE_OUT;
    }
    _ready_nodes.clear();
    _ready_nodes.resize(0);
    _npassedout += res.size();
    return res;
  }

  bool is_active() {
    // assert(!_ready_nodes.empty());
    // printf("_nfinished = %d, _npassedout = %d, _ready_nodes.size() = %lu\n", _nfinished, _npassedout,
    //        _ready_nodes.size());
    return (_nfinished < _npassedout) || _ready_nodes.size();
  }

  void done(const std::vector<bytes>& nodes) {
    for(const auto& node : nodes) {
      // TODO:
      //             # Check if we know about this node (it was added previously using add()
      //             if (nodeinfo := n2i.get(node)) is None:
      //                 raise ValueError(f"node {node!r} was not added using add()")
      assert(_node2info.contains(node));
      auto& ninfo = _node2info[node];
      int   stat  = ninfo.npred;
      if(stat != _NODE_OUT) {
        assert(0);
        // if(stat >= 0) {
        //   assert(0);
        // } else if(stat == _NODE_DONE) {
        //   assert(0);
        // } else {
        //   assert(0);
        // }
      }
      ninfo.npred = _NODE_DONE;

      for(const auto& suc : ninfo.succs) {
        _node2info[suc].npred -= 1;
        if(_node2info[suc].npred == 0) {
          _ready_nodes.push_back(suc);
        }
      }
      ++_nfinished;
    }
  }
};

struct _Graph {
  std::vector<bytes>         target_task_ids;
  TopologicalSorter          sorter;
  std::map<bytes, _TaskInfo> tasks;
  // TODO: Change it to ManyToManyDict
  // std::vector<std::pair<bytes, bytes>> depended_task_id_to_task_id;
  std::map<bytes, bytes> depended_task_id_to_task_id;
  bytes                  client;
  _GraphState            status;
  std::vector<bytes>     running_task_ids;

  // _Graph(std::vector<bytes> target_task_ids, TopologicalSorter sorter, std::map<bytes, _TaskInfo> tasks,
  //        std::map<bytes, bytes> depended_task_id_to_task_id, bytes client, _GraphState status,
  //        std::vector<bytes> running_task_ids)
  //     : target_task_ids(target_task_ids),
  //       sorter(sorter),
  //       tasks(tasks),
  //       depended_task_id_to_task_id(depended_task_id_to_task_id),
  //       client(client),
  //       status(status),
  //       running_task_ids(running_task_ids) {}
};

class graphtask_manager {
  std::shared_ptr<async_binder>    _binder;
  std::shared_ptr<async_connector> _binder_monitor;
  std::shared_ptr<client_manager>  _client_manager;
  std::shared_ptr<task_manager>    _task_manager;
  std::shared_ptr<object_manager>  _object_manager;

  std::queue<std::pair<bytes, bytes>> _unassigned;
  std::map<bytes, _Graph>             _graph_task_id_to_graph;
  std::map<bytes, bytes>              _task_id_to_graph_task_id;

public:
  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                     std::shared_ptr<client_manager> client_manager, std::shared_ptr<task_manager> task_manager,
                     std::shared_ptr<object_manager> object_manager) {
    _binder         = binder;
    _binder_monitor = binder_monitor;
    _client_manager = client_manager;
    _task_manager   = task_manager;
    _object_manager = object_manager;
  }

  void on_graph_task(zmq::message_t& source, capnp::ReaderFor<Message> message, kj::Array<capnp::word> raw);

  void on_graph_task_cancel(zmq::message_t& source, capnp::ReaderFor<Message> message);

  void routine();

  void mark_node_done(capnp::ReaderFor<TaskResult> task_result) {
    bytes task_id(task_result.getTaskId().asBytes().begin(), task_result.getTaskId().asBytes().end());
    // Not reference
    auto graph_task_id = _task_id_to_graph_task_id[task_id];
    _task_id_to_graph_task_id.erase(task_id);

    auto& graph_info = _graph_task_id_to_graph[graph_task_id];
    auto& task_info  = graph_info.tasks[task_id];

    for(const auto& result : task_result.getResults()) {
      bytes result_bytes(result.asBytes().begin(), result.asBytes().end());
      task_info.result_obj_ids.push_back(result_bytes);
    }

    switch(task_result.getStatus()) {
      case TaskStatus::SUCCESS:
        task_info.state = _NodeTaskState::Success;
        break;
      case TaskStatus::FAILED:
        task_info.state = _NodeTaskState::Failed;
        break;

      case TaskStatus::CANCELED:  // on intention
      case TaskStatus::NOT_FOUND:
        task_info.state = _NodeTaskState::Canceled;
        break;

      case TaskStatus::WORKER_DIED:
      case TaskStatus::NO_WORKER:
      case TaskStatus::INACTIVE:
      case TaskStatus::RUNNING:
      case TaskStatus::CANCELING:
      default:
        assert(0);
        break;
    }

    clean_intermediate_result(graph_task_id, task_id);
    graph_info.sorter.done({task_id});

    std::erase(graph_info.running_task_ids, task_id);
    if(std::ranges::find(graph_info.target_task_ids, task_id) != graph_info.target_task_ids.end()) {
      zmq::message_t client_msg(graph_info.client.data(), graph_info.client.data() + graph_info.client.size());
      capnp::MallocMessageBuilder msg;
      auto                        this_msg = msg.initRoot<Message>();
      this_msg.setTaskResult(task_result);
      _binder->send(std::move(client_msg), &msg);
    }
  }

  void clean_intermediate_result(bytes graph_task_id, bytes task_id) {
    auto& graph_info = _graph_task_id_to_graph[graph_task_id];
    auto& task_info  = graph_info.tasks[task_id];

    capnp::FlatArrayMessageReader reader(
        kj::ArrayPtr((capnp::word*)task_info.task.data(), task_info.task.size() / sizeof(capnp::word)));

    auto task_reader = reader.getRoot<Task>();

    for(const auto& argument : task_reader.getFunctionArgs()) {
      if(argument.getType() != Task::Argument::ArgumentType::TASK) continue;
      bytes argument_task_id(argument.getData().asBytes().begin(), argument.getData().asBytes().end());
      graph_info.depended_task_id_to_task_id.erase(argument_task_id);

      // TODO:
      // if there are no more usage for this result, delete these objects
      // but we will ignore that for now
    }
  }

  bool is_graph_sub_task(capnp::ReaderFor<TaskResult> task_result) {
    bytes task_id(task_result.getTaskId().asBytes().begin(), task_result.getTaskId().asBytes().end());
    return _task_id_to_graph_task_id.contains(task_id);
  }

  void on_graph_sub_task_done(capnp::ReaderFor<TaskResult> task_result) {
    bytes task_id(task_result.getTaskId().asBytes().begin(), task_result.getTaskId().asBytes().end());
    auto  graph_task_id = _task_id_to_graph_task_id[task_id];
    auto& graph_info    = _graph_task_id_to_graph[graph_task_id];
    if(graph_info.status == _GraphState::Cancelling) return;

    mark_node_done(task_result);

    if(task_result.getStatus() == TaskStatus::SUCCESS) {
      check_one_graph(graph_task_id);
      return;
    }

    // TODO: handle not successful case, which is cancel graph
  }

  // Graphtask is of type Message
  void add_new_graph(bytes client, bytes graphtask) {
    printf("add_new_graph\n");
    std::map<bytes, std::set<bytes>> graph;

    auto buffer = kj::heapArray<capnp::word>(graphtask.size() / sizeof(capnp::word));
    memcpy(buffer.asBytes().begin(), graphtask.data(), buffer.asBytes().size());
    capnp::FlatArrayMessageReader reader(buffer);
    auto                          graph_message = reader.getRoot<Message>().getGraphTask();
    auto                          task_id       = graph_message.getTaskId();
    zmq::message_t                client_message(client.data(), client.data() + client.size());
    // Pass to client_manager
    _client_manager->on_task_begin(client_message, task_id);

    std::map<bytes, _TaskInfo> tasks;
    std::map<bytes, bytes>     depended_task_id_to_task_id;

    bytes graphtask_id_bytes(graph_message.getTaskId().asBytes().begin(), graph_message.getTaskId().asBytes().end());
    for(const auto& task : graph_message.getGraph()) {
      bytes task_id_bytes(task.getTaskId().asBytes().begin(), task.getTaskId().asBytes().end());
      _task_id_to_graph_task_id[task_id_bytes] = graphtask_id_bytes;

      capnp::MallocMessageBuilder task_msg;
      task_msg.setRoot(task);
      auto  flat_task = capnp::messageToFlatArray(task_msg);
      bytes task_bytes(flat_task.asBytes().begin(), flat_task.asBytes().end());
      tasks[task_id_bytes] = _TaskInfo(_NodeTaskState::Inactive, task_bytes, {});

      std::set<bytes> required_task_ids;
      for(const auto& arg : task.getFunctionArgs()) {
        if(arg.getType() == ::Task::Argument::ArgumentType::TASK) {
          bytes arg_byte(arg.getData().asBytes().begin(), arg.getData().asBytes().end());
          required_task_ids.insert(arg_byte);
        }
      }

      for(const auto& required_task_id : required_task_ids) {
        depended_task_id_to_task_id[required_task_id] = task_id_bytes;
      }

      graph[task_id_bytes] = required_task_ids;
    }

    TopologicalSorter sorter(graph);

    sorter.prepare();

    std::vector<bytes> targets;
    for(const auto& target : graph_message.getTargets()) {
      targets.emplace_back(target.asBytes().begin(), target.asBytes().end());
    }

    _graph_task_id_to_graph[graphtask_id_bytes] = _Graph{
        .target_task_ids             = targets,
        .sorter                      = sorter,
        .tasks                       = tasks,
        .depended_task_id_to_task_id = depended_task_id_to_task_id,
        .client                      = client,
        .status                      = _GraphState::Running,
        .running_task_ids            = {},
    };

    check_one_graph(graphtask_id_bytes);
  }

  void check_one_graph(bytes graph_task_task_id) {
    _Graph& graph_info = _graph_task_id_to_graph[graph_task_task_id];
    // Sorter is not active <=> finished, need impl
    printf("check_onegraph\n");
    if(!graph_info.sorter.is_active()) {
      printf("is_active failed\n");
      finish_one_graph(graph_task_task_id, TaskStatus::SUCCESS);
      return;
    }
    auto ready_task_ids = graph_info.sorter.get_ready();
    if(ready_task_ids.empty()) {
      assert(0);
      return;
    }

    for(const auto& task_id : ready_task_ids) {
      auto& task_info = graph_info.tasks[task_id];
      task_info.state = _NodeTaskState::Running;
      graph_info.running_task_ids.push_back(task_id);

      auto                          task_bytes = task_info.task;
      capnp::FlatArrayMessageReader reader(
          kj::ArrayPtr((capnp::word*)task_bytes.data(), task_bytes.size() / sizeof(capnp::word)));
      auto task_reader = reader.getRoot<Task>();

      capnp::MallocMessageBuilder this_msg;
      auto                        this_msg_root      = this_msg.initRoot<Message>();
      auto                        this_msg_root_task = this_msg_root.initTask();
      this_msg_root_task.setTaskId(task_reader.getTaskId());
      this_msg_root_task.setSource(task_reader.getSource());
      this_msg_root_task.setMetadata(task_reader.getMetadata());
      this_msg_root_task.setFuncObjectId(task_reader.getFuncObjectId());

      auto task_args = this_msg_root_task.initFunctionArgs(task_reader.getFunctionArgs().size());
      for(int ii = 0; ii < task_reader.getFunctionArgs().size(); ++ii) {
        printf("in loop line %d\n", __LINE__);
        capnp::MallocMessageBuilder args_msg;
        task_args.setWithCaveats(ii, get_argument(graph_task_task_id, task_reader.getFunctionArgs()[ii], args_msg));
      }
      printf("line %d\n", __LINE__);
      auto           raw = capnp::messageToFlatArray(this_msg);
      zmq::message_t clientMsg(graph_info.client.data(), graph_info.client.data() + graph_info.client.size());

      printf("line %d\n", __LINE__);
      _task_manager->on_task_new(std::move(clientMsg), this_msg_root, std::move(raw));
      printf("line %d\n", __LINE__);
    }
  }

  capnp::ReaderFor<Task::Argument> get_argument(bytes graph_task_id, capnp::ReaderFor<Task::Argument> args,
                                                capnp::MallocMessageBuilder& msg) {
    printf("get_argument\n");
    if(args.getType() == Task::Argument::ArgumentType::OBJECT_I_D) return args;

    assert(args.getType() == Task::Argument::ArgumentType::TASK);
    bytes argument_task_id(args.getData().asBytes().begin(), args.getData().asBytes().end());
    auto& graph_info = _graph_task_id_to_graph[graph_task_id];
    auto& task_info  = graph_info.tasks[argument_task_id];
    printf("task_info.result_obj_ids.size() == %lu\n", task_info.result_obj_ids.size());
    assert(task_info.result_obj_ids.size() == 1);

    // capnp::MallocMessageBuilder msg;
    auto this_msg = msg.initRoot<Task::Argument>();
    this_msg.setType(Task::Argument::ArgumentType::OBJECT_I_D);
    this_msg.setData(
        capnp::Data::Reader(task_info.result_obj_ids.front().data(), task_info.result_obj_ids.front().size()));
    return this_msg;
  }

  // TODO: We cannot pass in TaskStatus directly here if we have task reroute
  void finish_one_graph(bytes graph_task_task_id, TaskStatus status) {
    _client_manager->on_task_finish(graph_task_task_id);
    auto&                       info = _graph_task_id_to_graph[graph_task_task_id];
    capnp::MallocMessageBuilder this_msg;
    auto                        this_msg_root             = this_msg.initRoot<Message>();
    auto                        this_msg_root_task_result = this_msg_root.initTaskResult();
    this_msg_root_task_result.setTaskId(capnp::Data::Reader(graph_task_task_id.data(), graph_task_task_id.size()));
    this_msg_root_task_result.setStatus(status);
    _binder->send(zmq::message_t(info.client.data(), info.client.data() + info.client.size()), &this_msg);
  }
};
