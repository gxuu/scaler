module;

#include <capnp/blob.h>
#include <capnp/common.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/common.h>

#include <memory>
#include <queue>
#include <ranges>
#include <zmq.hpp>

#include "async_binder.h"
#include "async_connector.h"
// #include "client_manager.h"
#include <set>

#include "common.capnp.h"
#include "message.capnp.h"
#include "utility/object_tracker.h"
#include "utility/typedefs.h"
// #include "worker_manager.h"

module scheduler;
import :object_manager;

// make it a data class for comparison and other wonderful thing

void object_manager::on_object_create(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  if(!_client_manager->has_client_id(message)) {
    return;
  }

  auto object_ids   = message.getObjectInstruction().getObjectContent().getObjectIds();
  auto object_types = message.getObjectInstruction().getObjectContent().getObjectTypes();
  auto object_names = message.getObjectInstruction().getObjectContent().getObjectNames();
  auto object_bytes = message.getObjectInstruction().getObjectContent().getObjectBytes();
  auto object_user  = message.getObjectInstruction().getObjectUser();

  size_t len = object_ids.size();

  for(auto ii = 0z; ii < len; ++ii) {
    _object_creation creation{
        .object_id      = bytes(object_ids[ii].asBytes().begin(), object_ids[ii].asBytes().end()),
        .object_creator = bytes(object_user.asBytes().begin(), object_user.asBytes().end()),
        .object_type    = object_types[ii],
        .object_name    = bytes(object_names[ii].asBytes().begin(), object_names[ii].asBytes().end()),
        .object_bytes   = {},
    };

    for(const auto& b : object_bytes[ii]) {
      creation.object_bytes.emplace_back(b.asBytes().begin(), b.asBytes().end());
    }

    on_add_object(std::move(creation));
  }
}
