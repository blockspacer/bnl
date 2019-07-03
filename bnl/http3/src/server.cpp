#include <bnl/http3/server.hpp>

#include <bnl/http3/error.hpp>

#include <bnl/util/error.hpp>

namespace bnl {
namespace http3 {

server::server(const log::api *logger)
    : logger_(logger),
      control_{ endpoint::server::control::sender(logger),
                endpoint::server::control::receiver(logger) }
{}

quic::event server::send(std::error_code &ec) noexcept
{
  endpoint::server::control::sender &control = control_.first;

  {
    quic::event event = control.send(ec);
    if (ec != error::idle) {
      return event;
    }
  }

  for (auto &entry : requests_) {
    uint64_t id = entry.first;
    endpoint::server::request::sender &request = entry.second.first;

    if (request.finished()) {
      continue;
    }

    quic::event event = request.send(ec);

    if (request.finished()) {
      requests_.erase(id);
    }

    if (ec != error::idle) {
      return event;
    }
  }

  THROW(error::idle);
}

nothing
server::recv(quic::event event, event::handler handler, std::error_code &ec)
{
  endpoint::server::control::receiver &control = control_.second;

  if (event.id == control.id()) {
    auto control_handler = [this, &handler](http3::event event,
                                            std::error_code &ec) {
      switch (event) {
        case event::type::settings:
          settings_.remote = event.settings;
          break;
        default:
          break;
      }

      handler(std::move(event), ec);
    };

    control.recv(std::move(event), control_handler, ec);

    return {};
  }

  auto match = requests_.find(event.id);
  if (match == requests_.end()) {
    endpoint::server::request::sender sender(event.id, logger_);
    endpoint::server::request::receiver receiver(event.id, logger_);

    TRY(receiver.start(ec));

    request request = std::make_pair(std::move(sender), std::move(receiver));
    requests_.insert(std::make_pair(event.id, std::move(request)));
  }

  endpoint::server::request::receiver &request = requests_.at(event.id).second;

  auto request_handler = [&handler](http3::event event, std::error_code &ec) {
    handler(std::move(event), ec);
  };

  request.recv(std::move(event), request_handler, ec);

  return {};
}

endpoint::handle server::response(uint64_t id, std::error_code &ec) noexcept
{
  CHECK(requests_.find(id) != requests_.end(), error::stream_closed);
  return requests_.at(id).first.handle();
}

} // namespace http3
} // namespace bnl
