#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace sshf::config {
	struct SshHost{
		std::string host;
		std::string hostname;
		std::string user;
		std::string port;

	};

	[[nodiscard]] auto parse_ssh_config(std::filesystem::path file_path) -> std::vector<SshHost>;
}