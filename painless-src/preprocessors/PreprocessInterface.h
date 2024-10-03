#include "utils/ClauseUtils.h"

class PreprocessInterface
{
public: 
     /// Constructor.
    PreprocessInterface() {}

    /// Destructor.
    virtual ~PreprocessInterface() {}

    virtual unsigned getVariablesCount() = 0;

    virtual unsigned getDivisionVariable() = 0;

    virtual void setInterrupt() = 0;

    virtual void unsetInterrupt() = 0;

    virtual void run() = 0;

    virtual void addInitialClauses(const std::vector<simpleClause> &clauses, unsigned nbVariables) = 0;

    virtual void loadFormula(const char *filename) = 0;

    virtual void printStatistics() = 0;

    virtual std::vector<simpleClause> getClauses() = 0;

    virtual std::size_t getClausesCount() = 0;

    virtual void printParameters() = 0;

    bool isInitialized() { return this->initialized; }

    virtual void restoreModel(std::vector<int> &model) = 0;

protected:
    /// @brief When the preprocessing can be started
    bool initialized = false;
};