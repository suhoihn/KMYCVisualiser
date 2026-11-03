#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>

using StringId = uint32_t;
class StringPool {
private:
    std::unordered_map<std::string, StringId> strToId;
    std::vector<std::string> strings; // id to str
public:
    StringId intern(const std::string& str);
    const std::string& get(StringId id) const;
    size_t size() const { return strings.size(); }
};
