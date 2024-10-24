#ifndef NUMERIC_CONSTANTS_H
#define NUMERIC_CONSTANTS_H

#include <cstdint>

/**
 * @file numeric_constants.h
 * @brief Defines constants for large numbers using constexpr for compile-time evaluation and type safety.
 *
 * This header provides constexpr constants for commonly used large numbers,
 * allowing for more readable code when dealing with quantities in the millions, billions, or trillions.
 * Using constexpr ensures these values are computed at compile-time and don't use runtime memory.
 */

/**
 * @brief Represents one million (1,000,000).
 */
constexpr std::int64_t MILLION = INT64_C(1000000);

/**
 * @brief Represents one billion (1,000,000,000).
 */
constexpr std::int64_t BILLION = INT64_C(1000000000);

/**
 * @brief Represents one trillion (1,000,000,000,000).
 */
constexpr std::int64_t TRILLION = INT64_C(1000000000000);

#endif // NUMERIC_CONSTANTS_H