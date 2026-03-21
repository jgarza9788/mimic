#pragma once

#include "mimic/types.hpp"

namespace mimic {

/**
 * @brief Minimal rules engine deciding default floating behavior.
 */
class RulesEngine {
public:
    /**
     * @brief Returns true when window should default to floating layer.
     */
    [[nodiscard]] bool should_float(const ManagedWindow& window) const;
};

} // namespace mimic
