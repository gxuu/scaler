#pragma once
#include <capnp/c++.capnp.h>
#include <capnp/common.h>
#include <capnp/generated-header-support.h>
#include <capnp/message.h>
#include <capnp/orphan.h>
#include <capnp/serialize.h>
#include <kj/common.h>

#include <functional>
#include <iterator>
#include <map>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "context.h"
#include "kj/array.h"
#include "message.capnp.h"

struct async_binder {
  using callback_t = std::function<void(zmq::message_t, kj::Array<capnp::word>)>;
  std::shared_ptr<context> _context;
  std::string              _name;
  callback_t               _callback;
  zmq::socket_t            _sock;
  std::string              _address;
  std::string              _identity;  // change this type later

  std::map<Message::Which, int> _received;
  std::map<Message::Which, int> _sent;

  async_binder(std::shared_ptr<context> context, std::string name)
      : _context(context), _name(name), _sock(zmq::socket_t(*_context->_context, zmq::socket_type::router)) {
    // FIXME: routing_id should be unique and not hard wired
    _sock.set(zmq::sockopt::routing_id, "hello");  //  will be set to identity later
    _sock.set(zmq::sockopt::sndhwm, 0);
    _sock.set(zmq::sockopt::rcvhwm, 0);

    // FIXME: socket binding address should be based on config
    _sock.bind("tcp://127.0.0.1:2345");
  }

  void build_channel(callback_t f) { _callback = f; }

  // maybe put that in the dtor
  void destroy(int linger = 0) {
    _sock.set(zmq::sockopt::linger, linger);
    _sock.close();
    _context->_context->close();
  }

  void routine() {
    std::vector<zmq::message_t> msgs;

    auto ret = zmq::recv_multipart(_sock, std::back_inserter(msgs));
    if(!ret) return;
    if(!valid_message()) {
      //
    }

    auto message = deserialize(msgs[1].data(), msgs[1].size());

    capnp::FlatArrayMessageReader reader(message);
    count_received(reader.getRoot<Message>().which());

    _callback(std::move(msgs[0]), std::move(message));
  }

  bool valid_message() { return true; }

  // because we need to keep the array alive
  kj::Array<capnp::word> deserialize(void* raw_bytes, int size) {
    // https://stackoverflow.com/questions/75006774/capn-proto-unaligned-data-error-while-trying-to-sendreceive-serialized-object
    auto buffer = kj::heapArray<capnp::word>(size / sizeof(capnp::word));
    memcpy(buffer.asBytes().begin(), raw_bytes, buffer.asBytes().size());
    return buffer;
    // capnp::FlatArrayMessageReader reader(buffer);
    // return reader.getRoot<Message>();
  }

  void send(zmq::message_t client, capnp::MallocMessageBuilder* message) {
    // count_send()
    auto flat_array = capnp::messageToFlatArray(*message);
    // Surprise: flat_array.size() != flat_array.asBytes().size()
    std::array<zmq::message_t, 2> send_msgs = {
        std::move(client), zmq::message_t(flat_array.asBytes().begin(), flat_array.asBytes().end())};
    zmq::send_multipart(_sock, send_msgs);
  }

  void get_status() {}

private:
  void count_received(Message::Which type) { ++_received[type]; }
  void count_send(Message::Which type) { ++_sent[type]; }
  void get_prefix() {}
};
