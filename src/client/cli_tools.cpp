#include "cli_tools.hpp"

namespace sshf::client {

	[[nodiscard]] auto get_ssh_conf_path() -> std::filesystem::path {
    	if (const char* home = std::getenv("HOME")) {
    	    return std::filesystem::path(home) / ".ssh" / "config";
    	}
	
    	return {};
	}
}