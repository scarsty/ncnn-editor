#pragma once

#include <map>
#include <string>
#include <vector>

// Parse ncnn-metadata.json without yaml-cpp dependency.
// Returns true when at least one layer entry is parsed.
bool load_ncnn_metadata_from_json(
    const std::vector<std::string>& candidates,
    std::map<std::string, std::map<int, std::string>>& int_to_string,
    std::map<std::string, std::map<std::string, int>>& string_to_int);
