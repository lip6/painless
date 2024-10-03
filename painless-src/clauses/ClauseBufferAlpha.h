// -----------------------------------------------------------------------------
// Copyright (C) 2017  Ludovic LE FRIOUX
//
// This file is part of PaInleSS.
//
// PaInleSS is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see <http://www.gnu.org/licenses/>.
// -----------------------------------------------------------------------------

#pragma once

#include "clauses/ClauseExchange.h"

// typedef Buffer<std::shared_ptr<ClauseExchange> > ClauseBuffer;

#include <atomic>
#include <memory>
#include <stdio.h>
#include <vector>



/// Clause buffer is a queue containning shared clauses.
class ClauseBufferAlpha
{
public:
   /// Constructor.
   ClauseBufferAlpha();

   /// Copy Constructor (needed for std::vector)
   ClauseBufferAlpha(ClauseBufferAlpha &otherbuff);

   /// @brief Move Constructor (needed for std::vector). Right now this constructor is a copy like constructor for safety reasons.
   ClauseBufferAlpha(ClauseBufferAlpha &&otherbuff);

   /// Destructor.
   ~ClauseBufferAlpha();

   /// Enqueue an element to the buffer.
   /// @param clause the element to add.
   void addClause(std::shared_ptr<ClauseExchange> clause);

   /// Enqueue multiple elements to the buffer
   /// @param clauses a vector containing the clauses to add in the database.
   void addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// @brief Enqueue element by following an increasing order
   /// @param clause the element to add
   // void addClauseOrderly(std::shared_ptr<ClauseExchange> clause);

   /// @brief Enqueue multiple elements in an increasing order
   /// @param clauses vector containing the elements to add
   // void addClausesOrderly(const std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// @brief Dequeue an element.
   /// @param clause a pointer to hold the dequeued value
   /// @return false if list is empty otherwise returns true
   bool getClause(std::shared_ptr<ClauseExchange> &clause);

   /// @brief Dequeue shared clauses.
   /// @param clauses a vector that will get the dequeued clauses
   void getClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// @brief Read a clause without consuming it, i.e. it doesn't update the head
   /// @param clause a pointer to hold the read value
   /// @return false if list is empty otherwise returns true
   bool readClause(std::shared_ptr<ClauseExchange> &clause);

   /// @brief Specialized function for clauseExchange
   /// @param clause the clause pointer to store the read value
   /// @return false if list is empty, otherwise true
   // bool readClause(std::shared_ptr<ClauseExchange> clause);

   /// @brief Read all the clauses present in the buffer without dequeing them
   /// @param clauses A vector to hold the clauses read.
   void readClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses);

   /// Return the current size of the buffer
   int size();

   /// To delete the head element and advance to next
   /// @return false if list is empty, otherwise true.
   bool pop_front();

   /// @brief To delete all the clauses present in the buffer
   void clear();

protected:
   typedef struct ListElement
   {
      std::shared_ptr<ClauseExchange> clause;

      std::atomic<ListElement *> next;         

      ListElement(std::shared_ptr<ClauseExchange> cls)
      {
         next   = NULL;
         clause = cls;
      }

      ~ListElement()
      {
      }
   } ListElement;

   typedef struct ListRoot
   {
      std::atomic<int> size;

      std::atomic<ListElement *> head;
      std::atomic<ListElement *> tail;
   } ListRoot;

   /// Root of producer/customers lists
   ListRoot buffer;
};