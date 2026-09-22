#include "ssh_config.hpp"

#include <fstream>
#include <cctype>

namespace sshf::config {
	static auto trim(std::string_view sv) -> std::string_view {
    	while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
    	    sv.remove_prefix(1);
    	}
    	while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
    	    sv.remove_suffix(1);
    	}
    	return sv;
	}

	[[nodiscard]] auto parse_ssh_config(std::filesystem::path file_path) -> std::vector<SshHost> {
		std::ifstream file(file_path);
    	if (!file.is_open()) {
    	    return {};
    	}
	
    	std::vector<SshHost> hosts;
    	std::string line;
    	SshHost* current_host = nullptr;
	
    	while (std::getline(file, line)) {
        	std::string_view sv = trim(line);
	
        	if (sv.empty() || sv.starts_with('#')) {
        	    continue;
        	}
	
        	size_t sep_pos = sv.find_first_of(" \t=");
        	if (sep_pos == std::string_view::npos) {
        	    continue;
        	}
	
        	std::string_view key = trim(sv.substr(0, sep_pos));
        	std::string_view value = trim(sv.substr(sep_pos + 1));
	
        	std::string key_lower;
        	key_lower.reserve(key.size());
        	for (char c : key) key_lower += static_cast<char>(std::tolower(c));
	
        	if (key_lower == "host") {
        	    if (value == "*") {
        	        current_host = nullptr;
        	        continue;
        	    }
        	    hosts.push_back(SshHost{.host = std::string(value)});
        	    current_host = &hosts.back();
        	} 
        	else if (current_host != nullptr) {
        	    if (key_lower == "hostname") {
        	        current_host->hostname = std::string(value);
        	    } else if (key_lower == "user") {
        	        current_host->user = std::string(value);
        	    } else if (key_lower == "port") {
        	        current_host->port = std::string(value);
        	    }
        	}
    	}

    	return hosts;
	}
}