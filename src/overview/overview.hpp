#pragma once

#include <optional>
#include <vector>

#include <xcb/xcb.h>

#include "layout/scroll_layout.hpp"
#include "model/workspace.hpp"

namespace scrollwm::overview {

struct OverviewState {
  bool active = false;
  int anchor_workspace_idx = 0;
  std::optional<int> selected_workspace_idx;
  std::optional<xcb_window_t> selected_client;
  std::optional<int> previous_workspace_idx;
  std::optional<xcb_window_t> previous_focused_client;
  float zoom_factor = 1.0F;
};

struct WorkspaceSceneEntry {
  int workspace_idx = 0;
  int origin_x = 0;
  int origin_y = 0;
  layout::Rect workspace_bounds{0, 0, 0, 0};
};

struct OverviewCamera {
  float scale = 1.0F;
  float center_x = 0.0F;
  float center_y = 0.0F;
  int viewport_width = 0;
  int viewport_height = 0;
};

struct OverviewRenderRect {
  int workspace_idx = 0;
  std::optional<xcb_window_t> client;
  layout::Rect rect{0, 0, 0, 0};
};

std::vector<WorkspaceSceneEntry> compute_workspace_scene_offsets(const std::vector<model::Workspace>& workspaces,
                                                                 int anchor_workspace_idx,
                                                                 int screen_width,
                                                                 int screen_height,
                                                                 int workspace_gap_px);

OverviewCamera build_overview_camera(const std::vector<WorkspaceSceneEntry>& scene,
                                     int anchor_workspace_idx,
                                     int viewport_width,
                                     int viewport_height,
                                     float margin_scale = 0.9F);

layout::Rect apply_overview_transform(const layout::Rect& world_rect, const OverviewCamera& camera);

std::vector<OverviewRenderRect> build_overview_render_rects(const std::vector<model::Workspace>& workspaces,
                                                            const std::vector<int>& scroll_offsets,
                                                            const layout::ScrollLayout& layout_engine,
                                                            const OverviewCamera& camera,
                                                            const std::vector<WorkspaceSceneEntry>& scene,
                                                            int screen_width,
                                                            int screen_height,
                                                            int gap,
                                                            int border_width,
                                                            int outer_padding);

void normalize_overview_state(OverviewState& state, const std::vector<model::Workspace>& workspaces);

}  // namespace scrollwm::overview
