#pragma once
#include "logistics/model/Order.h"
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace logistics {
struct OrderScenario {
    std::vector<Order> initial_orders;
    std::vector<Order> dynamic_orders;  // Stable ordering by release time.
};
class OrderConfigLoader {
public:
    static OrderScenario load(const std::filesystem::path& path);
    static OrderScenario read(std::istream& input);
};
}  // namespace logistics
