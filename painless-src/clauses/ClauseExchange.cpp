#include "ClauseExchange.h"
#include "utils/BloomFilter.h"
#include "utils/Logger.h"

ClauseExchange::ClauseExchange(unsigned size)
{
    this->lits.resize(size, 0);
    this->size = size;
    this->lbd = -1;
    this->from = -1;
}

ClauseExchange::ClauseExchange(std::vector<int> &&v_cls, unsigned lbd) : lits(v_cls), lbd(lbd)
{
    this->from = -1;
    this->size = lits.size();
    this->checksum = lookup3_hash_clause(this->lits);
}

ClauseExchange::ClauseExchange(std::vector<int> &&v_cls, unsigned lbd, unsigned from) : lits(v_cls), lbd(lbd), from(from)
{
    this->size = lits.size();
    this->checksum = lookup3_hash_clause(this->lits);
}

ClauseExchange::~ClauseExchange()
{
    LOGDEBUG2("ClauseExchange with ptr %p is destroyed", this);
}