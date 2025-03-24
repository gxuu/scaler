#pragma once

#include "zmq.hpp"
#include "zmq_addon.hpp"

class context {
public:
  std::shared_ptr<zmq::context_t> _context;

  // FIXME: io_thread count should be configurable
  context() : _context(new zmq::context_t(3)) {}
};
