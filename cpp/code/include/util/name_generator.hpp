/**
 * @file name_generator.hpp
 * @brief Defines a simple name generator for creating unique names.
 *
 * This file contains the declaration of the `NameGenerator` class, which is used
 * to generate unique names for temporary columns or other items by appending a
 * counter to a given prefix.
 */

#include <string>
#include <cstdint>

/**
 * @class NameGenerator
 * @brief A class for generating unique names.
 */
class NameGenerator {
private:
    uint32_t counter = 0;
public:
    /**
     * @brief Generates the next unique name with a given prefix.
     * @param prefix The prefix for the name.
     * @return A unique name (e.g., "prefix_0", "prefix_1").
     */
    // Generates names like "tmp_filter_0", "tmp_join_1", etc.
    std::string next(const std::string& prefix) {
        return prefix + "_" + std::to_string(counter++);
    }

    /**
     * @brief Gets the current name with a given prefix, without incrementing the counter.
     * @param prefix The prefix for the name.
     * @return The current name.
     */
    std::string cur(const std::string& prefix) {
        return prefix + "_" + std::to_string(counter);
    }

    /**
     * @brief Gets the next integer from the counter and increments it.
     * @return The next integer.
     */
    int next_int () {
        return counter++;
    }

    /**
     * @brief Gets the current integer from the counter without incrementing it.
     * @return The current integer.
     */
    int cur_int () {
        return counter;
    }
};
