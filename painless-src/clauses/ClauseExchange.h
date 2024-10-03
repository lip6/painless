#pragma once

#include <iostream>
#include <vector>

struct ClauseExchange
{
    ClauseExchange(unsigned size);

    ClauseExchange(std::vector<int> &&v_cls, unsigned lbd);

    ClauseExchange(std::vector<int> &&v_cls, unsigned lbd, unsigned from);

    ~ClauseExchange();

    /// LBD value of this clause.
    unsigned lbd;

    /// Id of the solver that has exported this clause.
    unsigned from;

    /// Size redundant with lits.size() kept temporarely
    unsigned size;

    /// Checksum of the clause
    size_t checksum;

    /// Literals of this clause.
    std::vector<int> lits;

    inline void printClauseExchange()
    {
        int size = this->lits.size();
        std::cout << "c Clause Exchange at" << this << ":" << std::endl;
        std::cout << " size: " << size;
        std::cout << " lbd: " <<  this->lbd;
        std::cout << " from: " << this->from;
        std::cout << " {";

        if (size > 0)
        {
            std::cout << this->lits[0];
            for (int i = 1; i < size; i++)
            {
                std::cout << ", " << this->lits[i];
            }
        }
        std::cout << "}" << std::endl;
    }
};
