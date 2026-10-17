#pragma once
#include "logistics/graph/Graph.h"
#include <filesystem>
#include <iosfwd>
#include <stdexcept>

namespace logistics {
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigLoader {
public:
    // A returned graph is fully validated. Failure never mutates an existing graph.
    static Graph load_network(const std::filesystem::path& path, bool require_course_size = false);
    static Graph read_network(std::istream& input, bool require_course_size = false);
    static void validate_course_size(const Graph& graph);
};
}  // namespace logistics
