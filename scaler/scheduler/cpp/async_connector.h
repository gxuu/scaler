#pragma once
#include <capnp/common.h>
#include <capnp/generated-header-support.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <capnp/serialize.h>

#include <zmq.hpp>

#include "context.h"

struct async_connector {
  std::shared_ptr<context> _context;
  std::string              _name;
  zmq::socket_type         _socket_type;
  std::string              _bind_or_connect;
  zmq::socket_t            _socket;

  async_connector(std::shared_ptr<context> context, std::string name, zmq::socket_type socket_type,
                  std::string bind_or_connect)
      : _context(context),
        _name(name),
        _socket_type(socket_type),
        _bind_or_connect(bind_or_connect),
        _socket(*_context->_context, _socket_type) {}

  void send(capnp::MallocMessageBuilder* message) {
    auto serialized = capnp::messageToFlatArray(*message);
    auto zmq_buf    = zmq::const_buffer(serialized.begin(), serialized.size());
    _socket.send(zmq_buf);
  }
};
