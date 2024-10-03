#include "ClauseUtils.h"
#include "BloomFilter.h"
#include "Logger.h"

/* no matter the order */
std::size_t hash_clause(std::vector<int> const &clause)
{
    if (clause.size() == 0)
        return 0;
    hash_t hash = lookup3_hash(clause[0]);
    int clauseSize = clause.size();
    for (int i = 1; i < clauseSize; i++)
    {
        hash ^= lookup3_hash(clause[i]);
    }
    return hash;
}

/* TODO a version for ordered clauses */
bool operator==(const std::vector<int> &vector1, const std::vector<int> &vector2)
{
    if(hash_clause(vector1) != hash_clause(vector2)) return false;


    LOGDEBUG1("Comparing two vectors with same hash vec1 %lu vs vec2 %lu", hash_clause(vector1), hash_clause(vector2));
    LOGCLAUSE(&vector1[0], vector1.size(), "vec1: ");
    LOGCLAUSE(&vector2[0], vector2.size(), "vec2: ");
    
    int size1 = vector1.size(), size2 = vector2.size();
    if (size1 != size2)
        return false;

    bool in;
    for (int i = 0; i < size1; i++)
    {
        in = false;
        for (int j = 0; j < size2; j++)
        {
            if (vector2[j] == vector1[i])
            {
                in = true;
                break;
            }
        }
        if (!in)
            return false;
    }
    return true;
}