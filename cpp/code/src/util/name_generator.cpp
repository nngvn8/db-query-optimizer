#include<string>
#include <cstdint>


class NameGenerator {
private:
    uint32_t counter = 0;
public:
    // Generates names like "tmp_filter_0", "tmp_join_1", etc.
    std::string next(const std::string& prefix) {
        return "tmp_" + prefix + "_" + std::to_string(counter++);
    }
};