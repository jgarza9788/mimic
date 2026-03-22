#include <csignal>
#include <cstdlib>
#include <filesystem>

#include "mimic/config.hpp"
#include "mimic/config_watcher.hpp"
#include "mimic/event_loop.hpp"
#include "mimic/ipc_server.hpp"
#include "mimic/logger.hpp"
#include "mimic/monitor_manager.hpp"
#include "mimic/rules_engine.hpp"
#include "mimic/window_manager.hpp"
#include "mimic/workspace_manager.hpp"
#include "mimic/xcb_backend.hpp"

namespace {

/**
 * @brief Global stop flag set by POSIX signal handlers.
 */
volatile sig_atomic_t g_stop = 0;

/**
 * @brief Signal handler that requests event loop shutdown.
 */
void on_signal(int) {
    g_stop = 1;
}

} // namespace

/**
 * @brief Entry point for mimic window manager executable.
 */
int main() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    const char* home_env = std::getenv("HOME");
    const std::filesystem::path home_path = home_env ? std::filesystem::path(home_env) : std::filesystem::path(".");

    mimic::ConfigLoader config_loader;
    auto config = config_loader.load_layered(MIMIC_SYSTEM_CONFIG_PATH, home_path / ".config/mimic/config.toml");

    mimic::Logger logger(mimic::LogLevel::Info);
    mimic::XcbBackend backend;
    mimic::WorkspaceManager workspace_manager;
    mimic::MonitorManager monitor_manager;
    mimic::RulesEngine rules_engine;
    mimic::CompositorBridge compositor_bridge;

    mimic::WindowManager wm(backend, workspace_manager, monitor_manager, rules_engine, compositor_bridge, logger,
                            config);
    if (!wm.initialize()) {
        return 1;
    }

    mimic::IpcServer ipc(config.ipc_socket_path);
    ipc.start([&wm](const std::string& command) { return wm.dispatch_command(command); });

    mimic::ConfigWatcher watcher(home_path / ".config/mimic/config.toml", [&]() {
        config = config_loader.load_layered(MIMIC_SYSTEM_CONFIG_PATH, home_path / ".config/mimic/config.toml");
        logger.log(mimic::LogLevel::Info, "Config reloaded");
    });

    mimic::EventLoop loop;
    loop.add_task([&]() { wm.poll_once(); });
    loop.add_task([&]() { ipc.poll_once(); });
    loop.add_task([&]() { watcher.poll(); });
    loop.add_task([&]() {
        if (g_stop) {
            loop.stop();
        }
    });
    loop.run();

    ipc.stop();
    return 0;
}
