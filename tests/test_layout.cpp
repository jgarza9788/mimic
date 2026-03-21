#include <cstdlib>
#include <iostream>

#include "mimic/layout_engine.hpp"
#include "test_suite.hpp"

/**
 * @brief Verifies insert-after-focus ordering for strip model.
 */
static void test_insert_after_focus() {
    mimic::LayoutEngine layout;
    layout.insert_after_focus(10);
    layout.insert_after_focus(20);
    layout.insert_after_focus(30);

    const auto& w = layout.windows();
    if (w.size() != 3 || w[0] != 10 || w[1] != 20 || w[2] != 30) {
        std::cerr << "insert-after-focus ordering failed\n";
        std::exit(1);
    }
}

/**
 * @brief Verifies left/right focus movement bounds.
 */
static void test_focus_movement() {
    mimic::LayoutEngine layout;
    layout.insert_after_focus(1);
    layout.insert_after_focus(2);
    layout.insert_after_focus(3);

    layout.move_focus(mimic::Direction::Left);
    if (layout.focused() != 2) {
        std::cerr << "focus left failed\n";
        std::exit(1);
    }

    layout.move_focus(mimic::Direction::Left);
    if (layout.focused() != 1) {
        std::cerr << "focus left boundary failed\n";
        std::exit(1);
    }

    layout.move_focus(mimic::Direction::Left);
    if (layout.focused() != 1) {
        std::cerr << "focus left hard boundary failed\n";
        std::exit(1);
    }
}

/**
 * @brief Verifies focused window placement is centered in viewport model.
 */
static void test_centering() {
    mimic::LayoutEngine layout;
    layout.insert_after_focus(11);
    layout.insert_after_focus(22);
    layout.insert_after_focus(33);

    const auto placements = layout.compute(1000, 600, 10);
    if (placements.size() != 3) {
        std::cerr << "placement count failed\n";
        std::exit(1);
    }

    if (placements[2].id != 33) {
        std::cerr << "focused placement tracking failed\n";
        std::exit(1);
    }

    const int expected_centered_x = (1000 - (1000 - 20)) / 2;
    if (placements[2].x != expected_centered_x) {
        std::cerr << "focused centering failed\n";
        std::exit(1);
    }
}

void run_layout_tests() {
    test_insert_after_focus();
    test_focus_movement();
    test_centering();
}
