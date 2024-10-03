#ifndef NDIST

#pragma once

#include "sharing/SharingEntity.hpp"
#include "clauses/ClauseDatabase.h"

#include <memory>

/// @brief The element saving the clauses toSend globally and the received ones
/// @details ClauseDatabases are shared_ptr to be able to use them by different Global Strategies in concurrence
/// \ingroup sharing
class GlobalDatabase : public SharingEntity
{
public:
    // Constructor
    GlobalDatabase(int entity_id, std::shared_ptr<ClauseDatabase> firstDB, std::shared_ptr<ClauseDatabase> secondDB);

    /// Destructor
    ~GlobalDatabase();

    //===================================================================================
    // Local Import/Export interface
    //===================================================================================

    /// Add a clause to clausesToSend == addToSend()
    bool importClause(std::shared_ptr<ClauseExchange> cls);

    /// Add multiple clauses to clausesToSend
    void importClauses(const std::vector<std::shared_ptr<ClauseExchange>> &v_cls);

    /// Give all the received clauses from receivedClauses, == getReceived()
    void exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls);

    /// @brief Give a limited number of clauses to not consume all the clauses and possibly loose them in the selection
    /// @param v_cls vector to hold the clauses selected
    /// @param totalSize the number of literals to return
    void exportClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls, unsigned totalSize);

    //===================================================================================
    // Add/Get Interface for GlobalSharingStrategy
    //===================================================================================

    /// @brief Add one clause to the received database
    /// @param cls the clause to add
    inline bool addReceivedClause(std::shared_ptr<ClauseExchange> cls)
    {
        return this->clausesReceived->addClause(cls);
    }

    /// @brief To add a set of clauses to clausesReceived
    /// @param v_cls the vector of clauses to add
    void addReceivedClauses(std::vector<std::shared_ptr<ClauseExchange>> &v_cls);

    /// @brief to get all the clauses in clausesToSend
    /// @param v_cls the vector that will contain the clauses from clausesToSend
    void getClausesToSend(std::vector<std::shared_ptr<ClauseExchange>> &v_cls);

    /// @brief to get a limited number of the clauses in clausesToSend
    /// @param v_cls the vector that will contain the clauses from clausesToSend
    /// @param literals_count the maximum number of literals to return
    void getClausesToSend(std::vector<std::shared_ptr<ClauseExchange>> &v_cls, unsigned literals_count);

    /// @brief to get the best clause in the clauseToSend database
    /// @param cls a double pointer, since I seek to get a pointer value
    /// @return true if found at least one clause to select, otherwise false
    inline bool getClauseToSend(std::shared_ptr<ClauseExchange> &cls)
    {
        return clausesToSend->giveOneClause(cls);
    }

    //==============================================================
    // Diverse methods
    //==============================================================

    /// @brief The method that will call the right visit method of the SharingEntityVisitor v
    /// @param v The SharingEntityVisitor that defines what should be done on this global sharer
    inline void accept(SharingEntityVisitor *v) override { v->visit(this); }

    // inline bool testStrengthening() override { return false; }

    inline void clear()
    {
        clausesToSend->deleteClauses();
        clausesReceived->deleteClauses();
    }

    inline void setMaxVar(unsigned maxVar_) { this->maxVar = maxVar_; }
    inline unsigned getMaxVar() { return this->maxVar; }

    void printStats();

protected:
    /// A collection of clauses that may be exported, the clauses are dereferenced before sharing.
    std::shared_ptr<ClauseDatabase> clausesToSend;

    /// A collection of received clauses, to be accessed by the sharers
    std::shared_ptr<ClauseDatabase> clausesReceived;

    /// To not accept certain clauses (SBVA)
    unsigned maxVar = 0;
};

#endif