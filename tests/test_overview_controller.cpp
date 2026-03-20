#include "mimic/MimicOverviewController.hh"

MIMIC_TEST(overview_controller_enter_and_pick_restores_inactive_state) {
  mimic::MimicOverviewController controller;
  std::vector<Window> windows = {1, 2, 3};

  MIMIC_ASSERT(controller.enter(windows, 1920, 1080));
  MIMIC_ASSERT(controller.state() == mimic::MimicOverviewController::State::kActive);

  auto picked = controller.pick_window(1);
  MIMIC_ASSERT(picked.has_value());
  MIMIC_ASSERT(picked.value() == 2);
  MIMIC_ASSERT(controller.state() == mimic::MimicOverviewController::State::kInactive);
}
