#include "overview/overview.hpp"

#include <algorithm>
#include <cmath>

namespace scrollwm::overview {

namespace {

const WorkspaceSceneEntry* find_scene_entry(const std::vector<WorkspaceSceneEntry>& scene, int workspace_idx) {
  for (const auto& entry : scene) {
    if (entry.workspace_idx == workspace_idx) {
      return &entry;
    }
  }
  return nullptr;
}

std::optional<xcb_window_t> first_client_for_workspace(const model::Workspace& ws) {
  if (ws.clients().empty()) {
    return std::nullopt;
  }
  return ws.clients().front().window;
}

}  // namespace

std::vector<WorkspaceSceneEntry> compute_workspace_scene_offsets(const std::vector<model::Workspace>& workspaces,
                                                                 int anchor_workspace_idx,
                                                                 int screen_width,
                                                                 int screen_height,
                                                                 int workspace_gap_px) {
  std::vector<WorkspaceSceneEntry> scene;
  if (workspaces.empty()) {
    return scene;
  }

  const int safe_screen_w = std::max(screen_width, 1);
  const int safe_screen_h = std::max(screen_height, 1);
  const int stride_x = safe_screen_w + std::max(workspace_gap_px, 0);

  scene.reserve(workspaces.size());
  for (const auto& ws : workspaces) {
    const int delta = ws.index() - anchor_workspace_idx;
    const int origin_x = delta * stride_x;
    const int origin_y = 0;
    scene.push_back(WorkspaceSceneEntry{
        .workspace_idx = ws.index(),
        .origin_x = origin_x,
        .origin_y = origin_y,
        .workspace_bounds = layout::Rect{origin_x, origin_y, safe_screen_w, safe_screen_h},
    });
  }
  return scene;
}

OverviewCamera build_overview_camera(const std::vector<WorkspaceSceneEntry>& scene,
                                     int anchor_workspace_idx,
                                     int viewport_width,
                                     int viewport_height,
                                     float margin_scale) {
  OverviewCamera camera;
  camera.viewport_width = std::max(viewport_width, 1);
  camera.viewport_height = std::max(viewport_height, 1);
  const float clamped_margin = std::clamp(margin_scale, 0.1F, 1.0F);

  if (scene.empty()) {
    camera.scale = 1.0F;
    camera.center_x = 0.0F;
    camera.center_y = 0.0F;
    return camera;
  }

  const WorkspaceSceneEntry* anchor = find_scene_entry(scene, anchor_workspace_idx);
  if (anchor == nullptr) {
    anchor = &scene.front();
  }

  const float center_x = static_cast<float>(anchor->workspace_bounds.x + anchor->workspace_bounds.width / 2);
  const float center_y = static_cast<float>(anchor->workspace_bounds.y + anchor->workspace_bounds.height / 2);

  float max_half_width = 1.0F;
  float max_half_height = 1.0F;
  for (const auto& entry : scene) {
    const float left = static_cast<float>(entry.workspace_bounds.x);
    const float right = static_cast<float>(entry.workspace_bounds.x + entry.workspace_bounds.width);
    const float top = static_cast<float>(entry.workspace_bounds.y);
    const float bottom = static_cast<float>(entry.workspace_bounds.y + entry.workspace_bounds.height);
    max_half_width = std::max(max_half_width, std::max(std::abs(left - center_x), std::abs(right - center_x)));
    max_half_height =
        std::max(max_half_height, std::max(std::abs(top - center_y), std::abs(bottom - center_y)));
  }

  const float scale_x = (static_cast<float>(camera.viewport_width) * 0.5F * clamped_margin) / max_half_width;
  const float scale_y = (static_cast<float>(camera.viewport_height) * 0.5F * clamped_margin) / max_half_height;

  camera.scale = std::min({1.0F, scale_x, scale_y});
  camera.center_x = center_x;
  camera.center_y = center_y;
  return camera;
}

layout::Rect apply_overview_transform(const layout::Rect& world_rect, const OverviewCamera& camera) {
  const float viewport_cx = static_cast<float>(camera.viewport_width) * 0.5F;
  const float viewport_cy = static_cast<float>(camera.viewport_height) * 0.5F;

  return layout::Rect{
      .x = static_cast<int>(std::lround((static_cast<float>(world_rect.x) - camera.center_x) * camera.scale +
                                        viewport_cx)),
      .y = static_cast<int>(std::lround((static_cast<float>(world_rect.y) - camera.center_y) * camera.scale +
                                        viewport_cy)),
      .width = std::max(1, static_cast<int>(std::lround(static_cast<float>(world_rect.width) * camera.scale))),
      .height = std::max(1, static_cast<int>(std::lround(static_cast<float>(world_rect.height) * camera.scale))),
  };
}

std::vector<OverviewRenderRect> build_overview_render_rects(const std::vector<model::Workspace>& workspaces,
                                                            const std::vector<int>& scroll_offsets,
                                                            const layout::ScrollLayout& layout_engine,
                                                            const OverviewCamera& camera,
                                                            const std::vector<WorkspaceSceneEntry>& scene,
                                                            int screen_width,
                                                            int screen_height,
                                                            int gap,
                                                            int border_width,
                                                            int outer_padding) {
  std::vector<OverviewRenderRect> output;
  if (workspaces.empty()) {
    return output;
  }

  for (size_t i = 0; i < workspaces.size(); ++i) {
    const auto& ws = workspaces[i];
    const WorkspaceSceneEntry* entry = find_scene_entry(scene, ws.index());
    if (entry == nullptr) {
      continue;
    }

    const auto transformed_ws = apply_overview_transform(entry->workspace_bounds, camera);
    output.push_back(OverviewRenderRect{.workspace_idx = ws.index(), .client = std::nullopt, .rect = transformed_ws});

    int offset = 0;
    if (i < scroll_offsets.size()) {
      offset = scroll_offsets[i];
    }
    const auto rects = layout_engine.compute(ws, screen_width, screen_height, gap, border_width, outer_padding, offset);

    const auto& clients = ws.clients();
    for (size_t c = 0; c < clients.size() && c < rects.size(); ++c) {
      auto world = rects[c];
      world.x += entry->origin_x;
      world.y += entry->origin_y;
      output.push_back(OverviewRenderRect{
          .workspace_idx = ws.index(),
          .client = clients[c].window,
          .rect = apply_overview_transform(world, camera),
      });
    }
  }

  return output;
}

void normalize_overview_state(OverviewState& state, const std::vector<model::Workspace>& workspaces) {
  if (workspaces.empty()) {
    state.active = false;
    state.anchor_workspace_idx = 0;
    state.selected_workspace_idx.reset();
    state.selected_client.reset();
    return;
  }

  const int min_idx = workspaces.front().index();
  const int max_idx = workspaces.back().index();
  state.anchor_workspace_idx = std::clamp(state.anchor_workspace_idx, min_idx, max_idx);

  if (state.selected_workspace_idx.has_value()) {
    state.selected_workspace_idx = std::clamp(*state.selected_workspace_idx, min_idx, max_idx);
  }

  if (!state.selected_workspace_idx.has_value()) {
    state.selected_workspace_idx = state.anchor_workspace_idx;
  }

  auto ws_it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& ws) {
    return ws.index() == *state.selected_workspace_idx;
  });
  if (ws_it == workspaces.end()) {
    state.selected_workspace_idx = state.anchor_workspace_idx;
    ws_it = std::find_if(workspaces.begin(), workspaces.end(), [&](const auto& ws) {
      return ws.index() == *state.selected_workspace_idx;
    });
  }

  if (!state.selected_client.has_value()) {
    if (ws_it != workspaces.end()) {
      state.selected_client = first_client_for_workspace(*ws_it);
    }
    return;
  }

  bool found = false;
  if (ws_it != workspaces.end()) {
    for (const auto& client : ws_it->clients()) {
      if (client.window == *state.selected_client) {
        found = true;
        break;
      }
    }
  }

  if (!found) {
    state.selected_client = (ws_it != workspaces.end()) ? first_client_for_workspace(*ws_it) : std::nullopt;
  }
}

}  // namespace scrollwm::overview
