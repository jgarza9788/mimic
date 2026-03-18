#include "model/workspace.hpp"

namespace scrollwm::model {

Workspace::Workspace(int index) : index_(index) {}

int Workspace::index() const { return index_; }

std::vector<Client>& Workspace::clients() { return clients_; }

const std::vector<Client>& Workspace::clients() const { return clients_; }

void Workspace::add_client(Client client) {
  clients_.push_back(client);
  focused_index_ = clients_.size() - 1;
}

void Workspace::remove_client(xcb_window_t window) {
  for (size_t i = 0; i < clients_.size(); ++i) {
    if (clients_[i].window == window) {
      clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(i));
      if (clients_.empty()) {
        focused_index_.reset();
      } else if (focused_index_.has_value()) {
        if (*focused_index_ >= clients_.size()) {
          focused_index_ = clients_.size() - 1;
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
