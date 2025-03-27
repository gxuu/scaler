module;

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/common.h>

#include <memory>
#include <queue>
#include <set>
#include <zmq.hpp>

#include "async_binder.h"
#include "async_connector.h"
#include "common.capnp.h"
#include "message.capnp.h"
#include "utility/object_tracker.h"
#include "utility/typedefs.h"
// #include "worker_manager.h"

export module scheduler:object_manager;
import :worker_manager;
import :client_manager;
export struct client_manager;

struct _object_creation {
  bytes                            object_id;
  bytes                            object_creator;
  ObjectContent::ObjectContentType object_type;
  bytes                            object_name;
  std::vector<bytes>               object_bytes;

  bytes get_object_key() { return object_id; }
};

struct object_tracker {
  std::vector<_object_creation>        creations;
  std::vector<std::pair<bytes, bytes>> _object_key_to_block;
  std::set<bytes>                      current;

  void add_object(_object_creation creation) { creations.push_back(creation); }

  void add_blocks_for_one_object(bytes object_key, std::set<bytes> blocks) {
    for(const auto& block : blocks) {
      _object_key_to_block.push_back({object_key, block});
    }
    current.merge(blocks);
  }

  void remove_one_block_for_objects(bytes id, zmq::message_t& source) {
    printf("remove_one_block_for_objects\n");
    return;
  }

  _object_creation get_object(bytes object_id) {
    for(int ii = 0; ii < creations.size(); ++ii) {
      if(creations[ii].get_object_key() == object_id) {
        return creations[ii];
      }
    }
    assert(false);
    return {};
  }
};

export struct object_manager {
  std::shared_ptr<async_binder>    _binder;
  std::shared_ptr<async_connector> _binder_monitor;
  std::shared_ptr<client_manager>  _client_manager;
  std::shared_ptr<worker_manager>  _worker_manager;

  std::queue<bytes> _queue_deleted_object_ids;

  object_tracker _object_storage;

  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                     std::shared_ptr<client_manager> client_manager, std::shared_ptr<worker_manager> worker_manager) {
    _binder         = binder;
    _binder_monitor = binder_monitor;
    _client_manager = client_manager;
    _worker_manager = worker_manager;
  }

  void on_object_instruction(zmq::message_t& source, capnp::ReaderFor<Message> message);

  void on_object_create(zmq::message_t& source, capnp::ReaderFor<Message> message);
  void on_del_objects(zmq::message_t& source, capnp::ReaderFor<Message> message);

  void on_object_request(zmq::message_t source, capnp::ReaderFor<Message> message);

  void on_add_object(_object_creation creation);

  void process_get_request(zmq::message_t source, capnp::ReaderFor<Message> message);

  void routine() { routine_send_objects_deletions(); }

  void routine_send_objects_deletions();
};
