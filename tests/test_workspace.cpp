#include <cstdlib>
#include <iostream>

#include "mimic/workspace_manager.hpp"
#include "test_suite.hpp"

/**
 * @brief Verifies dynamic workspace creation per monitor.
 */
static void test_workspace_creation() {
    mimic::WorkspaceManager manager;
    manager.ensure_monitor(1);
    manager.switch_to(1, 2);
    const auto names = manager.names(1);
    if (names.size() != 3) {
        std::cerr << "workspace auto-creation failed\n";
        std::exit(1);
    }
}

void run_workspace_tests() {
    test_workspace_creation();
}
