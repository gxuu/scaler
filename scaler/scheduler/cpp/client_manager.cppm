module;

#include <capnp/common.h>
#include <capnp/serialize.h>

#include <zmq.hpp>

#include "async_binder.h"
#include "async_connector.h"
#include "message.capnp.h"
#include "utility/one_to_many_dict.h"
#include "utility/typedefs.h"

export module scheduler:client_manager;

export struct task_manager;
export struct worker_manager;
export struct object_manager;

export struct client_manager {
  int _client_timeout_seconds;
  int _protect;

  std::shared_ptr<async_binder>    _binder;
  std::shared_ptr<async_connector> _binder_monitor;
  std::shared_ptr<object_manager>  _object_manager;
  std::shared_ptr<task_manager>    _task_manager;
  std::shared_ptr<worker_manager>  _worker_manager;

  // TODO: investigate how python code uses one_to_many_dict
  one_to_many_dict<bytes, bytes>                     _client_to_task_ids;
  std::map<bytes, std::pair<float, ClientHeartbeat>> _client_last_seen;

  client_manager(int client_timeout_seconds = 1, int protect = 1);

  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                     std::shared_ptr<object_manager> object_manager, std::shared_ptr<task_manager> task_manager,
                     std::shared_ptr<worker_manager> worker_manager);

  bool has_client_id(capnp::ReaderFor<Message> message);

  void on_heartbeat(zmq::message_t source, capnp::ReaderFor<Message> message);

  void on_client_disconnect(zmq::message_t& source, capnp::ReaderFor<Message> message);

  void __on_client_disconnect(zmq::message_t& client_id);

  void on_task_begin(zmq::message_t& client_id, capnp::Data::Reader task_id);

  bytes on_task_finish(bytes task_id);
};
