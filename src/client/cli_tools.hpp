#include <config/ssh_config.hpp>

#include <filesystem>

namespace sshf::client {	
	[[nodiscard]] auto get_ssh_conf_path() -> std::filesystem::path;
}