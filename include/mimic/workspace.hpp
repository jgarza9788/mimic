#pragma once

#include <string>

#include "mimic/layout_engine.hpp"

namespace mimic {

/**
 * @brief Workspace state bound to one monitor and one strip layout.
 */
class Workspace {
public:
    /**
     * @brief Constructs workspace with user-facing name.
     */
    explicit Workspace(std::string name);

    /**
     * @brief Returns workspace name.
     */
    [[nodiscard]] const std::string& name() const;

    /**
     * @brief Returns mutable layout engine for updates.
     */
    [[nodiscard]] LayoutEngine& layout();

    /**
     * @brief Returns immutable layout engine for reads.
     */
    [[nodiscard]] const LayoutEngine& layout() const;

private:
    std::string name_;
    LayoutEngine layout_;
};

} // namespace mimic
