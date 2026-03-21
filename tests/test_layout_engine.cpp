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

MIMIC_TEST(layout_engine_scrollable_strip_keeps_window_width_stable) {
  mimic::MimicLayoutEngine engine;
  engine.set_window_order({1, 2, 3, 4});
  const auto frames = engine.compute_frames(1920, 1080);

  MIMIC_ASSERT(frames.size() == 4);
  MIMIC_ASSERT(frames[0].width == frames[1].width);
  MIMIC_ASSERT(frames[1].width == frames[2].width);
  MIMIC_ASSERT(frames[0].x < frames[1].x);
  MIMIC_ASSERT(frames[1].x < frames[2].x);
}

MIMIC_TEST(layout_engine_honors_layout_options) {
  mimic::MimicLayoutEngine engine;
  engine.set_layout_options({40, 10, 200, 0.5});
  engine.set_window_order({1, 2});
  const auto frames = engine.compute_frames(1000, 700);

  MIMIC_ASSERT(frames.size() == 2);
  MIMIC_ASSERT(frames[0].x == 10);
  MIMIC_ASSERT(frames[0].y == 10);
  MIMIC_ASSERT(frames[0].height == 680);
  MIMIC_ASSERT(frames[0].width == 490);
  MIMIC_ASSERT(frames[1].x == 540);
}
