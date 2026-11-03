#include "StringPool.hpp"

StringId StringPool::intern(const std::string& str) {
    auto it = strToId.find(str);
   
    if (it == strToId.end()) {
        StringId newId = strings.size();
        strToId[str] = newId;
        strings.push_back(str);
        return newId;
    }

    return it->second;
}

const std::string& StringPool::get(StringId id) const {
    return strings[id];
}
