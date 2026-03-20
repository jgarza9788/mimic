#pragma once

#include <cstddef>
#include <vector>

namespace mimic {

struct Workspace {
  std::size_t id;
  std::size_t window_count;
};

class MimicWorkspaceModel {
 public:
  MimicWorkspaceModel();

  std::size_t workspace_count() const { return workspaces_.size(); }
  std::size_t active_workspace_index() const { return active_workspace_index_; }

  std::size_t create_workspace();
  void set_active_workspace(std::size_t index);

  void add_window(std::size_t workspace_index);
  void remove_window(std::size_t workspace_index);

  bool maybe_remove_empty_workspace(std::size_t workspace_index);
  static bool should_remove_empty_workspace(std::size_t total_workspaces, std::size_t windows_in_workspace);

 private:
  std::vector<Workspace> workspaces_;
  std::size_t active_workspace_index_;
  std::size_t next_workspace_id_;
};

}  // namespace mimic
