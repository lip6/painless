#pragma once

// forward declarations
class SharingEntity;
class GlobalDatabase;
class SolverCdclInterface;

/// @brief SharingEntityVisitor interface implemented in SharingStrategy
/// \ingroup sharing
class SharingEntityVisitor
{
public:
    /// @brief Default behavior of all sharing_entity is a must
    /// @param sh_entity the sharing entity to interact with (visit)
    virtual void visit(SharingEntity *sh_entity) = 0;

#ifndef NDIST
    /// @brief The default behavior of any sharingEntity if this strategy
    /// @param g_base the global database to interact with (visit)
    virtual void visit(GlobalDatabase *g_base) = 0;
#endif

    /// @brief  Send only to the reducer and receive only from it
    /// @details Each producer fill its database, then a selection is done and sent only if the producer or the consumer is a reducer. In other words, no direct sharing between two classic solvers or between two reducers.
    /// @param solver the solver to interact with (visit)
    virtual void visit(SolverCdclInterface *solver) = 0;
};