#include "sshf.hpp"
#include "client/cli_tools.hpp"
#include "client/net/cli_net.hpp"
#include "config/ssh_config.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace sshf {

using namespace ftxui;

// ------------------------- config and data helpers -------------------------------

static auto _get_display_names(const std::vector<config::SshHost>& hosts) -> std::vector<std::string> {
    std::vector<std::string> names;
    names.reserve(hosts.size());
    for (const auto& h : hosts) {
        names.push_back(h.host);
    }
    return names;
}

static auto _filter_hosts(const std::vector<config::SshHost>& all_hosts, std::string_view query) -> std::vector<config::SshHost> {
    if (query.empty()) return all_hosts;

    std::vector<config::SshHost> result;
    for (const auto& h : all_hosts) {
        if (h.host.find(query) != std::string_view::npos ||
            h.hostname.find(query) != std::string_view::npos) {
            result.push_back(h);
        }
    }
    return result;
}

static void _start_session(const config::SshHost* host) {
    if (!host) return;
    std::string command = "ssh " + host->host;
    std::system(command.c_str());
}

static auto _ensure_ssh_config() -> std::vector<config::SshHost> {
    const auto conf_path = client::get_ssh_conf_path();
    auto hosts = config::parse_ssh_config(conf_path);
    if (!hosts.empty()) return hosts;

    std::cerr << "Error: SSH config is empty or missing.\n";

    while (true) {
        std::cout << "Create default SSH config? [y/n]: ";
        std::string answer;
        if (!std::getline(std::cin, answer)) return {};

        if (answer == "y" || answer == "Y") {
            const char* home_dir = std::getenv("HOME");
            if (!home_dir) return {};

            std::filesystem::path ssh_dir = std::filesystem::path(home_dir) / ".ssh";
            std::filesystem::path config_path = ssh_dir / "config";

            try {
                std::filesystem::create_directories(ssh_dir);
                std::filesystem::permissions(
                    ssh_dir,
                    std::filesystem::perms::owner_all,
                    std::filesystem::perm_options::replace);

                std::ofstream file(config_path, std::ios::app);
                if (!file.is_open()) {
                    std::cerr << "Error: Could not create config file.\n";
                    return {};
                }

                file << "\nHost example\n\tHostName localhost\n\tPort 22\n";
                file.close();

                std::filesystem::permissions(
                    config_path,
                    std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                    std::filesystem::perm_options::replace);

                std::cout << "Config successfully created!\n";
                return config::parse_ssh_config(conf_path);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Filesystem error: " << e.what() << "\n";
                return {};
            }
        }

        if (answer == "n" || answer == "N") {
            std::cout << "Aborting...\n";
            return {};
        }
    }
}

// ------------------------------- rendering -------------------------------------

static auto _render_line_row(std::string_view label, std::string_view val, Color val_col) -> Element {
    return hbox({
        text(std::string(label)) | color(Color::Blue) | bold,
        text(std::string(val)) | color(val_col)
    });
}

static auto _render_host_details(const config::SshHost* active, double current_ping) -> Element {
    return vbox({
        _render_line_row(" Host:     ", active ? active->host : "-", Color::White),
        _render_line_row(" HostName: ", active ? active->hostname : "-", Color::GrayLight),
        _render_line_row(" User:     ", active ? active->user : "-", Color::GrayLight),
        _render_line_row(" Port:     ", active ? active->port : "-", Color::GrayLight),
        separator(),
        vbox({
            hbox({
                text(" Status:   ") | color(Color::White) | bold,
                text(current_ping > 0 ? "OK " : (current_ping < 0 ? "BAD " : "-"))
                    | color(current_ping > 0 ? Color::Green : Color::Red),
            }),
            hbox({
                text(" Ping:     ") | color(Color::White) | bold,
                text(current_ping > 0 ? std::format("{:.2f} ms", current_ping) : "-")
                    | color(Color::GrayLight),
            })
        })
    });
}

static auto _render_ping_graph(
    const std::vector<float>& ping_history,
    double current_ping,
    std::mutex& ping_mutex) -> Element 
{
    return graph([&](int width, int height) {
        std::vector<int> result(width, -1);
        std::lock_guard<std::mutex> lock(ping_mutex);

        if (current_ping < 0 || ping_history.empty()) {
            return result;
        }

        constexpr int bar_width = 2;
        constexpr int spacing = 2;
        constexpr int step = bar_width + spacing;

        float max_ping = 50.0f;
        for (float val : ping_history) {
            if (val > max_ping) max_ping = val;
        }

        const int history_size = static_cast<int>(ping_history.size());
        for (int i = 0; i < history_size; ++i) {
            int history_idx = history_size - 1 - i;
            int x = width - bar_width - (i * step);

            if (x + bar_width <= 0) break;

            float val = ping_history[history_idx];
            int bar_height = 0;

            if (val > 0) {
                float norm = std::clamp(val / max_ping, 0.0f, 1.0f);
                bar_height = static_cast<int>(norm * static_cast<float>(height - 1));
            }

            for (int w = 0; w < bar_width; ++w) {
                int px = x + w;
                if (px >= 0 && px < width) {
                    result[px] = bar_height;
                }
            }
        }
        return result;
    }) | flex | color(Color::Green);
}

static auto _render_footer() -> Element {
    return hbox({
        text(" Enter") | bold, text(" Connect  "),
        text(" q/Esc") | bold, text(" Quit  "),
        text(" ↑/↓")   | bold, text(" Navigate  "),
        filler(),
        text("sshf v1.0.0 ") | dim
    }) | bgcolor(Color::Blue) | color(Color::RGB(255, 255, 255));
}

static auto _render_main_layout(
    const Component& input_field,
    const Component& menu,
    bool no_results,
    const config::SshHost* active_host,
    double current_ping,
    const std::vector<float>& ping_history,
    std::mutex& ping_mutex) -> Element 
{
    return vbox({
        hbox({
            // left panel. hosts list
            vbox({
                input_field->Render(),
                separator(),
                no_results 
                    ? text(" no results :(") | dim 
                    : menu->Render() | vscroll_indicator | frame
            }) | border | size(WIDTH, EQUAL, 30),

            //right panel. host info
            vbox({
                _render_host_details(active_host, current_ping),
                separator(),
                vbox({
                    text(" Ping Graph") | color(Color::Blue) | bold,
                    separator(),
                    _render_ping_graph(ping_history, current_ping, ping_mutex)
                }) | flex
            }) | border | flex
        }) | flex,

        _render_footer()
    });
}

// ----------------------------- app thread --------------------------------------------------

[[nodiscard]] auto sshf_main([[maybe_unused]] std::span<const std::string_view> args) -> int {
    std::vector<config::SshHost> all_hosts = _ensure_ssh_config();
    if (all_hosts.empty()) {
        return EXIT_FAILURE;
    }

    bool running = true;
    while (running) {
        auto screen = ScreenInteractive::Fullscreen();

        std::vector<config::SshHost> filtered_hosts = all_hosts;
        std::vector<std::string> display_names = _get_display_names(filtered_hosts);

        std::string input_text;
        int selected = 0;

        std::atomic<double> current_ping{-1.0};
        std::mutex ping_mutex;
        std::vector<float> ping_history;
        constexpr size_t kMaxPingHistory = 100;

        const config::SshHost* target_host = nullptr;

        auto selected_host = [&]() -> const config::SshHost* {
            if (!filtered_hosts.empty() && selected >= 0 && selected < static_cast<int>(filtered_hosts.size())) {
                return &filtered_hosts[selected];
            }
            return nullptr;
        };

        // search init
        auto input = Input(&input_text, "  search...");
        auto search_box = CatchEvent(input, [&](Event event) {
            if (input->OnEvent(event)) {
                std::erase(input_text, '\n');
                std::erase(input_text, '\r');

                filtered_hosts = _filter_hosts(all_hosts, input_text);
                display_names = _get_display_names(filtered_hosts);

                if (filtered_hosts.empty()) {
                    selected = 0;
                } else if (selected >= static_cast<int>(filtered_hosts.size())) {
                    selected = static_cast<int>(filtered_hosts.size()) - 1;
                }
                return true;
            }
            return false;
        });

        // hosts menu
        MenuOption menu_option;
        menu_option.on_enter = [&] {
            if (auto* host = selected_host()) {
                target_host = host;
                screen.Exit();
            }
        };

        auto menu = Menu(&display_names, &selected, menu_option);
        auto layout = Container::Vertical({search_box, menu});

        auto main_container = CatchEvent(layout, [&](Event event) {
            if (event == Event::Escape || event == Event::Character('q')) {
                running = false;
                screen.Exit();
                return true;
            }
            return false;
        });

        // render
        auto renderer = Renderer(main_container, [&] {
            return _render_main_layout(
                input,
                menu,
                filtered_hosts.empty(),
                selected_host(),
                current_ping.load(),
                ping_history,
                ping_mutex
            );
        });

        //ping thread
        std::jthread net([&](std::stop_token stop_tok) {
            int last_select = -1;

            while (!stop_tok.stop_requested()) {
                const config::SshHost* curr = selected_host();

                if (curr) {
                    if (selected != last_select) {
                        last_select = selected;
                        std::lock_guard<std::mutex> lock(ping_mutex);
                        ping_history.clear();
                    }

                    auto ping_opt = client::net::get_host_ping(curr->hostname);
                    double p = ping_opt.value_or(-1.0);
                    current_ping.store(p);

                    {
                        std::lock_guard<std::mutex> lock(ping_mutex);
                        if (ping_history.size() >= kMaxPingHistory) {
                            ping_history.erase(ping_history.begin());
                        }
                        ping_history.push_back(static_cast<float>(p));
                    }

                    screen.PostEvent(Event::Custom);
                }

                for (int i = 0; i < 20 && !stop_tok.stop_requested() && selected == last_select; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });

        screen.Loop(renderer);

        net.request_stop();

        if (target_host) {
            _start_session(target_host);
        } else {
            running = false;
        }
    }

    return EXIT_SUCCESS;
}

}