#include "model/workspace.hpp"

#include <algorithm>

namespace scrollwm::model {

Workspace::Workspace(int index) : index_(index) {}

int Workspace::index() const { return index_; }

std::vector<Client>& Workspace::clients() { return clients_; }

const std::vector<Client>& Workspace::clients() const { return clients_; }

void Workspace::add_client(Client client) {
  clients_.push_back(client);
  focused_index_ = clients_.size() - 1;
  note_focus(client.window);
}

void Workspace::remove_client(xcb_window_t window) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].window == window) {
      const bool removed_was_focused = focused_index_.has_value() && *focused_index_ == i;
      clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(i));
      focus_history_.erase(std::remove(focus_history_.begin(), focus_history_.end(), window), focus_history_.end());
      if (clients_.empty()) {
        focused_index_.reset();
      } else if (focused_index_.has_value()) {
        if (*focused_index_ > i) {
          focused_index_ = *focused_index_ - 1;
        } else if (*focused_index_ == i) {
          if (i >= clients_.size()) {
            focused_index_ = clients_.size() - 1;
          } else {
            focused_index_ = i;
          }
        } else if (*focused_index_ >= clients_.size()) {
          focused_index_ = clients_.size() - 1;
        }
      }
      if (!clients_.empty() && !removed_was_focused && !focused_index_.has_value()) {
        for (auto it = focus_history_.rbegin(); it != focus_history_.rend(); ++it) {
          for (size_t j = 0; j < clients_.size(); ++j) {
            if (clients_[j].window == *it) {
              focused_index_ = j;
              return;
            }
          }
        }
      }
      return;
    }
  }
}

std::optional<size_t> Workspace::focused_index() const { return focused_index_; }

void Workspace::focus_index(size_t idx) {
  if (idx < clients_.size()) {
    focused_index_ = idx;
    note_focus(clients_[idx].window);
  }
}

void Workspace::focus_next() {
  if (clients_.empty()) {
    focused_index_.reset();
    return;
  }
  if (!focused_index_.has_value()) {
    focused_index_ = 0;
    return;
  }
  focused_index_ = (*focused_index_ + 1) % clients_.size();
  note_focus(clients_[*focused_index_].window);
}

void Workspace::focus_prev() {
  if (clients_.empty()) {
    focused_index_.reset();
    return;
  }
  if (!focused_index_.has_value()) {
    focused_index_ = 0;
    return;
  }

  if (*focused_index_ == 0) {
    focused_index_ = clients_.size() - 1;
  } else {
    focused_index_ = *focused_index_ - 1;
  }
  note_focus(clients_[*focused_index_].window);
}

void Workspace::focus_urgent() {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].urgent) {
      focused_index_ = i;
      note_focus(clients_[i].window);
      return;
    }
  }
}

void Workspace::note_focus(xcb_window_t window) {
  focus_history_.erase(std::remove(focus_history_.begin(), focus_history_.end(), window), focus_history_.end());
  focus_history_.push_back(window);
}

void Workspace::reorder_focused_forward() {
  if (!focused_index_.has_value() || clients_.empty()) {
    return;
  }
  const size_t idx = *focused_index_;
  if (idx + 1 >= clients_.size()) {
    return;
  }
  std::swap(clients_[idx], clients_[idx + 1]);
  focused_index_ = idx + 1;
}

void Workspace::reorder_focused_backward() {
  if (!focused_index_.has_value() || clients_.empty()) {
    return;
  }
  const size_t idx = *focused_index_;
  if (idx == 0) {
    return;
  }
  std::swap(clients_[idx], clients_[idx - 1]);
  focused_index_ = idx - 1;
}

Client* Workspace::focused_client() {
  if (!focused_index_.has_value() || *focused_index_ >= clients_.size()) {
    return nullptr;
  }
  return &clients_[*focused_index_];
}

const Client* Workspace::focused_client() const {
  if (!focused_index_.has_value() || *focused_index_ >= clients_.size()) {
    return nullptr;
  }
  return &clients_[*focused_index_];
}

int Workspace::scroll_offset() const { return scroll_offset_; }

void Workspace::set_scroll_offset(int offset) { scroll_offset_ = offset; }

}  // namespace scrollwm::model
