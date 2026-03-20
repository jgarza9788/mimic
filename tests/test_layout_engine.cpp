#include "mimic/MimicLayoutEngine.hh"

MIMIC_TEST(layout_engine_focus_cycle_with_three_windows) {
  mimic::MimicLayoutEngine engine;
  engine.set_window_order({11, 12, 13});

  MIMIC_ASSERT(engine.current_window().has_value());
  MIMIC_ASSERT(engine.current_window().value() == 11);
  MIMIC_ASSERT(engine.focus_next().value() == 12);
  MIMIC_ASSERT(engine.focus_next().value() == 13);
  MIMIC_ASSERT(engine.focus_next().value() == 11);
}

MIMIC_TEST(layout_engine_remove_updates_focus) {
  mimic::MimicLayoutEngine engine;
  engine.set_window_order({21, 22});
  engine.focus_next();
  engine.remove_window(22);

  MIMIC_ASSERT(engine.current_window().has_value());
  MIMIC_ASSERT(engine.current_window().value() == 21);
}
