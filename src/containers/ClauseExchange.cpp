#include "ClauseExchange.hpp"
#include "utils/Logger.hpp"
#include <sstream>

ClauseExchange::ClauseExchange(const csize_t size_, const lbd_t lbd_, const plid_it from_)
	: lbd(lbd_)
	, from(from_)
	, size(size_)
	, refCounter(0)
{
	// Some solvers can generate non unit clause with lbd == 1
	if (size > 1 && lbd == 1) {
		lbd = 2;
	}
	assert(size > 1 && lbd > 1 ||
		   size == 1 && (lbd == 0 || lbd == 1)); // is there a solver that uses lbd = 1 for units ?
}

ClauseExchangePtr
ClauseExchange::create(const csize_t size, const lbd_t lbd, const plid_it from)
{
	// Allocate memory for the object and the flexible array member
	void* memory = std::malloc(sizeof(ClauseExchange) + size * sizeof(lit_t));
	if (!memory)
		throw std::bad_alloc();

	// Use placement new to construct the object
	return ClauseExchangePtr(new (memory) ClauseExchange(size, lbd, from));
}

ClauseExchangePtr
ClauseExchange::create(const lit_t* begin, const lit_t* end, const lbd_t lbd, const plid_it from)
{
	// Create a new ClauseExchange object
	auto ce = create(end - begin, lbd, from);

#ifndef NDEBUG
	for (const lit_t* it = begin; it != end; it++)
		assert(*it);
#endif

	// Copy the literals from the vector to the flexible array member
	std::copy(begin, end, ce->begin());

	return ce;
}

ClauseExchangePtr
ClauseExchange::create(const std::vector<lit_t>& v_cls, const lbd_t lbd, const plid_it from)
{
	// Create a new ClauseExchange object
	auto ce = create(v_cls.size(), lbd, from);

#ifndef NDEBUG
	for (auto lit : v_cls)
		assert(lit);
#endif

	// Copy the literals from the vector to the flexible array member
	std::copy(v_cls.begin(), v_cls.end(), ce->begin());

	return ce;
}

std::string
ClauseExchange::toString() const
{
	std::ostringstream oss;
	oss << "@" << this << ": ";
	oss << " refCounter: " << refCounter.load(std::memory_order_relaxed);
	oss << ", size: " << size;
	oss << ", lbd: " << lbd;
	oss << ", from: " << from;
	oss << ", lits: {";
	if (size > 0) {
		oss << lits[0];
		for (csize_t i = 1; i < size; i++) {
			oss << ", " << lits[i];
		}
	}
	oss << "}";
	return oss.str();
}
