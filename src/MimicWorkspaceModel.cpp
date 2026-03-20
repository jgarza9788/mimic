#include "mimic/MimicWorkspaceModel.hh"

#include <algorithm>

namespace mimic {

MimicWorkspaceModel::MimicWorkspaceModel() : active_workspace_index_(0), next_workspace_id_(1) {
  workspaces_.push_back({next_workspace_id_++, 0});
}

std::size_t MimicWorkspaceModel::create_workspace() {
  workspaces_.push_back({next_workspace_id_++, 0});
  return workspaces_.size() - 1;
}

void MimicWorkspaceModel::set_active_workspace(std::size_t index) {
  if (index < workspaces_.size()) {
    active_workspace_index_ = index;
  }
}

void MimicWorkspaceModel::add_window(std::size_t workspace_index) {
  if (workspace_index < workspaces_.size()) {
    ++workspaces_[workspace_index].window_count;
  }
}

void MimicWorkspaceModel::remove_window(std::size_t workspace_index) {
  if (workspace_index < workspaces_.size() && workspaces_[workspace_index].window_count > 0) {
    --workspaces_[workspace_index].window_count;
  }
}

bool MimicWorkspaceModel::should_remove_empty_workspace(std::size_t total_workspaces,
                                                        std::size_t windows_in_workspace) {
  return total_workspaces > 1 && windows_in_workspace == 0;
}

bool MimicWorkspaceModel::maybe_remove_empty_workspace(std::size_t workspace_index) {
  if (workspace_index >= workspaces_.size()) {
    return false;
  }

  if (!should_remove_empty_workspace(workspaces_.size(), workspaces_[workspace_index].window_count)) {
    return false;
  }

  workspaces_.erase(workspaces_.begin() + static_cast<long>(workspace_index));

  if (workspaces_.empty()) {
    workspaces_.push_back({next_workspace_id_++, 0});
    active_workspace_index_ = 0;
  } else if (active_workspace_index_ >= workspaces_.size()) {
    active_workspace_index_ = workspaces_.size() - 1;
  } else if (workspace_index <= active_workspace_index_ && active_workspace_index_ > 0) {
    --active_workspace_index_;
  }

  return true;
}

}  // namespace mimic
