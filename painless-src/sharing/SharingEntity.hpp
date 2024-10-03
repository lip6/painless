#pragma once

#include "sharing/SharingEntityVisitor.h"
#include "clauses/ClauseExchange.h"
#include "../Entity.hpp"

#include <vector>
#include <memory>
#include <atomic>

/// @brief A base class representing the entities that can exchange clauses via the local sharer(s)
/// \ingroup sharing
class SharingEntity : public Entity
{
public:
    SharingEntity(int _id) : Entity(_id) {}

    virtual ~SharingEntity() {}

    /// @brief to get clauses from this sharing entity
    /// @param v_clauses the vector to fill with clauses
    virtual void exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_clauses) = 0;

    /// @brief to add a clause to this sharing entity
    /// @param clause the clause to add
    /// @return true if clause was imported, otherwise false
    virtual bool importClause(std::shared_ptr<ClauseExchange> clause) = 0;

    /// @brief to add multiple clauses to this sharing entity
    /// @param v_clauses the vector with the clauses to add
    virtual void importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &v_clauses) = 0;

    /**
     * @brief to set the lbdLimit on clauses to export
     * @param lbdValue the new lbdLimit value
    */
    virtual void setLbdLimit(unsigned lbdValue)
    {
        this->lbdLimit = lbdValue;
    }

    /// @brief The method used to call the SharingEntityVisitor's method
    /// @param v The SharingEntityVisitor that implements methods on this object
    virtual void accept(SharingEntityVisitor *v) { v->visit(this); }

    /// Lbd value the clauses mustn't exceed at export
    std::atomic<unsigned> lbdLimit;
};