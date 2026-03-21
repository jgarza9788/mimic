#include "mimic/workspace.hpp"

namespace mimic {

Workspace::Workspace(std::string name) : name_(std::move(name)) {}

const std::string& Workspace::name() const {
    return name_;
}

LayoutEngine& Workspace::layout() {
    return layout_;
}

const LayoutEngine& Workspace::layout() const {
    return layout_;
}

} // namespace mimic
