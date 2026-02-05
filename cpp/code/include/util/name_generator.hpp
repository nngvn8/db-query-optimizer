#include <string>
#include <cstdint>


class NameGenerator {
private:
    uint32_t counter = 0;
public:
    // Generates names like "tmp_filter_0", "tmp_join_1", etc.
    std::string next(const std::string& prefix) {
        return prefix + "_" + std::to_string(counter++);
    }
    std::string cur(const std::string& prefix) {
        return prefix + "_" + std::to_string(counter);
    }
    int next_int () {
        return counter++;
    }
    int cur_int () {
        return counter;
    }
};