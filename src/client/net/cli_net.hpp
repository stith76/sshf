#pragma once
#include <optional>
#include <string>

namespace sshf::client::net {

auto get_host_ping(const std::string& host, int port = 22) -> std::optional<double>;

}