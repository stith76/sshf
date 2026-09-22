#include <span>
#include <string_view>

namespace sshf {
	[[nodiscard]] auto sshf_main(std::span<const std::string_view> args) -> int;
}