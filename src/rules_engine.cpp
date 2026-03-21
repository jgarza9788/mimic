#include "mimic/rules_engine.hpp"

namespace mimic {

bool RulesEngine::should_float(const ManagedWindow& window) const {
    if (window.fullscreen) {
        return true;
    }
    if (window.app_class.find("dialog") != std::string::npos) {
        return true;
    }
    if (window.app_class.find("popup") != std::string::npos) {
        return true;
    }
    return window.floating;
}

} // namespace mimic
