#include <cstdlib>
#include <fstream>
#include <iostream>

#include "mimic/config.hpp"
#include "test_suite.hpp"

/**
 * @brief Verifies user config overrides layered default values.
 */
static void test_config_layering() {
    const std::string sys_path = "/tmp/mimic_test_sys.toml";
    const std::string user_path = "/tmp/mimic_test_user.toml";

    {
        std::ofstream sys(sys_path);
        sys << "gaps = 10\n";
        sys << "dim_inactive = true\n";
    }
    {
        std::ofstream user(user_path);
        user << "gaps = 4\n";
    }

    mimic::ConfigLoader loader;
    auto config = loader.load_layered(sys_path, user_path);
    if (config.gaps != 4 || !config.dim_inactive) {
        std::cerr << "config layering failed\n";
        std::exit(1);
    }
}

void run_config_tests() {
    test_config_layering();
}

/**
 * @brief Test entry point used by CTest.
 */
int main() {
    run_layout_tests();
    run_workspace_tests();
    run_config_tests();
    std::cout << "all tests passed\n";
    return 0;
}
