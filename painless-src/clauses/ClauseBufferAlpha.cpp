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

#include "ClauseBufferAlpha.h"

#include <iostream>

//-------------------------------------------------
// Constructor & Destructor
//-------------------------------------------------
ClauseBufferAlpha::ClauseBufferAlpha()
{
   ListElement *node = new ListElement(NULL);
   buffer.head = buffer.tail = node;
   buffer.size = 0;
   // std::cout << "c ";
   // std::cout << "Buffer created with head = tail = " << node << std::endl;
}

ClauseBufferAlpha::ClauseBufferAlpha(ClauseBufferAlpha &otherbuff)
{
   // Init
   ListElement *node = new ListElement(NULL);
   buffer.head = buffer.tail = node;
   buffer.size = 0;

   // Copy
   std::vector<std::shared_ptr<ClauseExchange>> tmp;
   otherbuff.readClauses(tmp); // reads only without consuming
   this->addClauses(tmp);
}

ClauseBufferAlpha::ClauseBufferAlpha(ClauseBufferAlpha &&otherbuff)
{
   // not safe
   /*buffer.buffer = otherbuff.buffer;
   ListElement *node = new ListElement(NULL);
   otherbuff.buffer.head = otherbuff.buffer.tail = node;
   otherbuff.buffer.size = 0;*/

   // not optimal but safe
   // Init
   ListElement *node = new ListElement(NULL);
   buffer.head = buffer.tail = node;
   buffer.size = 0;

   // Cut
   std::vector<std::shared_ptr<ClauseExchange>> tmp;
   otherbuff.getClauses(tmp); // get with consuming
   this->addClauses(tmp);
}

ClauseBufferAlpha::~ClauseBufferAlpha()
{
}

//-------------------------------------------------
//  Add clause(s)
//-------------------------------------------------
void ClauseBufferAlpha::addClause(std::shared_ptr<ClauseExchange> clause)
{
   ListElement *tail, *next;
   ListElement *node = new ListElement(clause);

   while (true) // essayer jusqu'à réussir à ajouter à la fin
   {
      tail = buffer.tail;
      // buffer peut avoir eu un ajout tail!=buffer.tail du coup on reessaie
      next = tail->next;
      // next aura une autre valeur que null si un element a été ajouté
      if (tail == buffer.tail) // verifier que mon tail est le bon
      {
         if (next == NULL) // verifier que le tail pris été dernier à un certain moment
         {
            if (tail->next.compare_exchange_strong(next, node)) // si deux thread arrive ici au meme temps, le 2eme a faire le CAS va voir que son tail->next n'est plus le même
            {
               buffer.size++;
               break;
            }
         }
         else // si le buffer.tail a eté mis à jour entre le tail et next.
         {
            // là next peut être null si plusieurs get ? Non, vu que get vérifie head == tail. Et s'il met à jour tail et consomme son next, le cas va échouer vue que buffer.tail != tail
            buffer.tail.compare_exchange_strong(tail, next); // metter à jour le buffer tail si il ne l'est pas
         }
      }
   }

   // mettre à jour le tail si ce dernier n'a pas eté mis à jour par un autre via le else dans la boucle
   buffer.tail.compare_exchange_strong(tail, node);
}

void ClauseBufferAlpha::addClauses(const std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   int clausesSize = clauses.size();
   for (int i = 0; i < clausesSize; i++)
   {
      addClause(clauses[i]);
   }
}

//-------------------------------------------------
//  Get/Read clause(s)
//-------------------------------------------------
bool ClauseBufferAlpha::getClause(std::shared_ptr<ClauseExchange> &clause)
{
   ListElement *head, *tail, *next;

   while (true)
   {
      head = buffer.head;
      tail = buffer.tail;
      next = head->next;

      if (head == buffer.head) // essayer jusqu'à avoir le bon head
      {
         if (head == tail) // verifier si un seul element
         {
            if (next == NULL) // on peut rater un ajout qui a été fait entre temps, pas grave ?
               return false;

            // le compare est vrai que si buffer.tail n'a pas changer autrement dit == head d'ou l'utilisation de head->next
            // et aussi que next n'est pas null
            buffer.tail.compare_exchange_strong(tail, next); // un noeud a ete ajouté mais tail n'a pas encore eté mis à jour
         }
         else
         {
            clause = next->clause; // return next pour eviter de retourner le NULL du début et pour avoir une chaine vide si head == tail

            if (buffer.head.compare_exchange_strong(head, next)) // si deux thread arrivent ici, head ne sera changer qu'une seule fois.
            {
               break; // un seul thread fera le break, le 2eme refera un tour de boucle
            }
         }
      }
   }

   delete head;

   buffer.size--;

   return true;
}

void ClauseBufferAlpha::getClauses(std::vector<std::shared_ptr<ClauseExchange>>  &clauses)
{
   std::shared_ptr<ClauseExchange> cls;

   int nClauses = size();
   int nClausesGet = 0;

   while (getClause(cls) && nClausesGet < nClauses)
   {
      clauses.push_back(cls);
      nClausesGet++;
   }
}

bool ClauseBufferAlpha::readClause(std::shared_ptr<ClauseExchange> &clause)
{
   ListElement *head, *tail, *next;

   // the most important thing is to not read an empty list or a dereferenced element.
   while (true)
   {
      head = buffer.head;
      tail = buffer.tail;
      next = head->next;

      if (head == buffer.head)
      {
         if (head == tail)
         {
            if (next == NULL)
               return false;

            buffer.tail.compare_exchange_strong(tail, next); // un noeud a ete ajouté mais tail n'a pas encore eté mis à jour
         }
         else
         {
            clause = next->clause;
         }
      }
   }

   // ne delete pas le head et ne decremente pas le size

   return true;
}

void ClauseBufferAlpha::readClauses(std::vector<std::shared_ptr<ClauseExchange>> &clauses)
{
   std::shared_ptr<ClauseExchange> cls;

   int nClauses = size();
   int nClausesGet = 0;

   while (readClause(cls) && nClausesGet < nClauses) // s'arrete si tout est lu ou si liste est vidée par un autre thread
   {
      clauses.push_back(cls);
      nClausesGet++;
   }
}

//-------------------------------------------------
//  Get size of the buffer.
//-------------------------------------------------
int ClauseBufferAlpha::size()
{
   return buffer.size;
}

//-------------------------------------------------
//      Delete all the clauses of the buffer
//-------------------------------------------------
bool ClauseBufferAlpha::pop_front()
{
   ListElement *head, *tail, *next;

   while (true)
   {
      head = buffer.head;
      tail = buffer.tail;
      next = head->next;

      if (head == buffer.head)
      {
         if (head == tail) // check if empty
         {
            if (next == NULL) // is empty
               return false;

            buffer.tail.compare_exchange_strong(tail, next); // un noeud a ete ajouté mais tail n'a pas encore eté mis à jour
         }
         else
         {
            if (buffer.head.compare_exchange_strong(head, next)) // si deux thread arrivent simultanemant ici, head ne sera changer qu'une seule fois.
            {
               break; // un seul thread fera le break, le 2eme refera un tour de boucle
            }
         }
      }
   }

   delete head;

   buffer.size--;

   return true;
}

void ClauseBufferAlpha::clear()
{
   int current_size = size();
   while (pop_front() && current_size > 0) // stops if buffer is empty or deleted all the previously present elements
   {
      current_size--;
   }
}