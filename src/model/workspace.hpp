#pragma once

#include <optional>
#include <vector>

#include "model/client.hpp"

namespace scrollwm::model {

class Workspace {
 public:
  explicit Workspace(int index);

  int index() const;
  void set_index(int index);
  std::vector<Client>& clients();
  const std::vector<Client>& clients() const;

  void add_client(Client client);
  void remove_client(xcb_window_t window);

  std::optional<size_t> focused_index() const;
  void focus_index(size_t idx);
  void focus_next();
  void focus_prev();
  void focus_urgent();
  void note_focus(xcb_window_t window);
  void reorder_focused_forward();
  void reorder_focused_backward();

  Client* focused_client();
  const Client* focused_client() const;

  int scroll_offset() const;
  void set_scroll_offset(int offset);

 private:
  int index_;
  std::vector<Client> clients_;
  std::optional<size_t> focused_index_;
  int scroll_offset_ = 0;
  std::vector<xcb_window_t> focus_history_;
};

// Workspace IDs are compact contiguous indices [0..N-1]. When empty
// workspaces are deleted, later workspaces are reindexed.
void ensure_workspace_exists(std::vector<Workspace>& workspaces, int idx);
void cleanup_empty_workspaces(std::vector<Workspace>& workspaces,
                              std::vector<int>& tracked_workspace_indices);

}  // namespace scrollwm::model
