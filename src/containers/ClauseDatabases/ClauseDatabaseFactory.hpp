#pragma once

#include "containers/ClauseDatabase.hpp"

#include <memory>
#include <string>

/**
 * @brief Factory class for creating clause database instances.
 * 
 * This factory provides a centralized way to create different types of clause databases
 * based on character option parameters. Configuration parameters for each database type
 * are stored as static members and can be initialized separately.
 */
class ClauseDatabaseFactory
{
public:
    /**
     * @brief Initialize factory parameters.
     * 
     * @param maxClauseSize Maximum clause size for the database.
     * @param maxCapacity Maximum literal capacity for database (Mallob and SingleBuffer).
     * @param mallobMaxPartitioningLbd Maximum LBD value for Mallob database partitioning.
     * @param mallobMaxFreeSize Maximum free size for Mallob database.
     */
    static void initialize(
        unsigned int maxClauseSize, 
        size_t maxCapacity,
        int mallobMaxPartitioningLbd,
        int mallobMaxFreeSize
    );

    /**
     * @brief Create a database from a character option.
     * 
     * @param dbTypeChar Character option representing the database type:
     *        's' - SingleBuffer, 'p' - PerSize, 'e' - PerEntity, 'm' - Mallob
     * @return std::shared_ptr<ClauseDatabase> A shared pointer to the created database.
     */
    static std::shared_ptr<ClauseDatabase> createDatabase(char dbTypeChar);

    /**
     * @brief Check if a character is a valid database type option.
     * 
     * @param dbTypeChar Character to check.
     * @return bool True if the character is a valid database type option.
     */
    static bool isValidDatabaseType(char dbTypeChar);

public:
    // Static configuration parameters
    static unsigned int s_maxClauseSize;
    static size_t s_maxCapacity;
    static int s_mallobMaxPartitioningLbd;
    static int s_mallobMaxFreeSize;
    static bool s_initialized;
};