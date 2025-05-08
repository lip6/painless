#ifndef _SAGA_H_
#define _SAGA_H_

#include "containers/SimpleTypes.hpp"
#include <algorithm>
#include <boost/compute/detail/lru_cache.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <math.h>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "painless.hpp"
#include "utils/Logger.hpp"
#include <climits>

namespace saga {

// class Formula
// {
//   public:
// 	Formula() = default;
// 	Formula(int numVariables, int numClauses)
// 		: numVariables(numVariables)
// 		, numClauses(numClauses)
// 		, fix(numVariables + 1, 0)
// 		, fixed_vars(numVariables + 1, 0)
// 	{
// 	}

// 	Formula(const Formula& other)
// 		: numVariables(other.numVariables)
// 		, numClauses(other.numClauses)
// 		, fix(other.fix)
// 		, fixed_vars(other.fixed_vars)
// 		,

// 		degree_centrality_variables(other.degree_centrality_variables)
// 	{
// 	}

// 	~Formula() = default;

// 	Formula& operator=(const Formula& other)
// 	{
// 		numVariables = other.numVariables;
// 		numClauses = other.numClauses;
// 		fix = other.fix;
// 		fixed_vars = other.fixed_vars;
// 		degree_centrality_variables = other.degree_centrality_variables;
// 		return *this;
// 	}

// 	int getNumVariables() const { return numVariables; }

// 	int getNumClauses() const { return numClauses; }

// 	std::vector<bool>& getFix() { return fix; }

// 	std::vector<bool>& getFixedVars() { return fixed_vars; }

// 	void setNumVariables(int numVariables)
// 	{
// 		this->numVariables = numVariables;
// 		// this->var_lit_count.resize(numVariables + 1);
// 		this->fix.resize(numVariables + 1);
// 		this->fixed_vars.resize(numVariables + 1);
// 	}

// 	void setNumClauses(int numClauses)
// 	{
// 		this->numClauses = numClauses;
// 		// this->clauses.resize(numClauses);
// 		// this->clause_lit_count.resize(numClauses);
// 		// this->clause_delete.resize(numClauses);
// 	}

// 	std::vector<unsigned>& get_degree_centrality_variables() { return degree_centrality_variables; }
// 	void set_degree_centrality_variables(std::vector<unsigned>& degree_centrality_variables)
// 	{
// 		this->degree_centrality_variables = degree_centrality_variables;
// 	}

// 	std::vector<std::pair<unsigned, unsigned>>& get_binary_clauses() { return binary_clauses; }
// 	void set_binary_clauses(std::vector<std::pair<unsigned, unsigned>>& binary_clauses)
// 	{
// 		this->binary_clauses = binary_clauses;
// 	}
// 	void add_binary_clause(unsigned lit1, unsigned lit2) { binary_clauses.emplace_back(lit1, lit2); }

// 	std::vector<bool> fix;		  // fixed variable.
// 	std::vector<bool> fixed_vars; // Values of  Fixed variables.

//   private:
// 	std::vector<unsigned> degree_centrality_variables;
// 	std::vector<std::pair<unsigned, unsigned>> binary_clauses;
// 	int numVariables;
// 	int numClauses;
// };

// class Solution
// {
//   public:
// 	Solution(const Solution& other)
// 		: solution(other.solution)
// 		, fitness(other.fitness) //, unsatisfying_variables(other.unsatisfying_variables)
// 	{
// 	}
// 	Solution(std::vector<unsigned> solution_, int fitness_)
// 		: solution(solution_)
// 		, fitness(fitness_) //, unsatisfying_variables()
// 	{
// 	}
// 	Solution(size_t solution_size, int nclauses)
// 		: solution(solution_size)
// 		, fitness(nclauses) //, unsatisfying_variables()
// 	{
// 		solution.reserve(solution_size);
// 	}

// 	Solution()
// 		: solution()
// 		, fitness(1000000)
// 	{
// 	}

// 	~Solution() {}

// 	// Operators overloading
// 	Solution& operator=(const Solution& other)
// 	{
// 		if (this != &other) {
// 			solution = other.solution;
// 			fitness = other.fitness;
// 			// unsatisfying_variables = other.unsatisfying_variables;
// 		}

// 		return *this;
// 	}

// 	bool operator==(const Solution& s) const { return solution == s.solution && fitness == s.fitness; }

// 	bool operator!=(const Solution& s) const { return !(*this == s); }

// 	const unsigned& operator[](std::size_t i) const { return solution[i]; }
// 	unsigned& operator[](std::size_t i) { return solution[i]; }

// 	std::string toString() const
// 	{
// 		std::ostringstream oss;
// 		oss << "(" << this->getFitness() << ") ";
// 		for (int i = 1; i < size(); ++i)
// 			oss << (solution[i] ? "" : "-") << i << " ";
// 		oss << std::endl;
// 		return oss.str();
// 	}

// 	void setRandomSolution(std::mt19937& rng)
// 	{
// 		std::uniform_int_distribution<int> dist(0, 1);
// 		for (std::size_t i = 1; i < solution.size(); ++i) {
// 			solution[i] = dist(rng);
// 		}
// 	}

// 	// Getters and setters
// 	int getFitness() const { return fitness; }
// 	void setFitness(int fitness_) { fitness = fitness_; }
// 	std::size_t size() const { return solution.size(); }
// 	void resize(std::size_t size_) { solution.resize(size_); }
// 	std::vector<unsigned> getSolution() const { return solution; }
// 	void setSolution(std::vector<unsigned> solution_) { solution = solution_; }

//   private:
// 	std::vector<unsigned> solution;
// 	// std::vector<unsigned> unsatisfying_variables; // Vector that maps unsatisfying variables to the number of clauses
// 	// they do not satisfy
// 	int fitness;
// };

class Solution
{
  public:
	Solution(const Solution& other)
		: solution(other.solution)
		, fitness(other.fitness)
		, combined_fitness(other.combined_fitness)
		, mutation_rate(other.mutation_rate)
		, crossover_rate(other.crossover_rate) //, unsatisfying_variables(other.unsatisfying_variables)
	{
	}

	// Solution(const Solution &other) = default;
	// Solution(Solution &&other) noexcept = default;

	Solution(std::vector<unsigned> solution_, int fitness_, float pm, float pc)
		: solution(solution_)
		, fitness(fitness_)
		, mutation_rate(pm)
		, crossover_rate(pc)
		, combined_fitness(0.0) //, unsatisfying_variables()
	{
	}
	Solution(size_t solution_size, int nclauses)
		: solution(solution_size)
		, fitness(nclauses)
		, combined_fitness(0.0)
	{
		solution.reserve(solution_size);
	}

	Solution()
		: solution()
		, fitness(UINT_MAX)
		, combined_fitness(0.0)
	{
	}

	~Solution() {}

	// Operators overloading
	Solution& operator=(const Solution& other)
	{
		if (this != &other) {
			solution = other.solution;
			fitness = other.fitness;
			mutation_rate = other.mutation_rate;
			crossover_rate = other.crossover_rate;
			combined_fitness = other.combined_fitness;
		}

		return *this;
	}

	bool operator==(const Solution& s) const
	{
		return solution == s.solution && fitness == s.fitness && mutation_rate == s.mutation_rate &&
			   crossover_rate == s.crossover_rate &&
			   std::abs(combined_fitness - s.combined_fitness) <
				   1e-4; // && unsatisfying_variables == s.unsatisfying_variables;
	}

	bool operator!=(const Solution& s) const { return !(*this == s); }

	const unsigned& operator[](std::size_t i) const { return solution[i]; }
	unsigned& operator[](std::size_t i) { return solution[i]; }

	std::string toString() const
	{
		std::ostringstream oss;
		for (int i = 0; i < size(); ++i)
			oss << (solution[i] ? "" : "-") << i << " ";
		oss << std::endl;
		return oss.str();
	}

	void setRandomSolution(std::mt19937& rng)
	{
		std::uniform_int_distribution<int> dist(0, 1);
		for (std::size_t i = 0; i < solution.size(); ++i) {
			solution[i] = dist(rng);
		}
	}

	// Getters and setters
	float getMutationRate() const { return mutation_rate; }
	void setMutationRate(float mutation_rate_) { mutation_rate = mutation_rate_; }

	float getCrossoverRate() const { return crossover_rate; }
	void setCrossoverRate(float crossover_rate_) { crossover_rate = crossover_rate_; }

	int getFitness() const { return fitness; }
	void setFitness(int fitness_) { fitness = fitness_; }
	// Overload for combined fitness
	void setFitness(double fitness_) { combined_fitness = fitness_; }

	double getCombinedFitness() const { return combined_fitness; }
	void setCombinedFitness(double combined_fitness_) { combined_fitness = combined_fitness_; }

	std::size_t size() const { return solution.size(); }
	void resize(std::size_t size_) { solution.resize(size_); }
	std::vector<unsigned> getSolution() const { return solution; }
	void setSolution(std::vector<unsigned> solution_) { solution = solution_; }

	double hamming_distance(const Solution& other) const
	{
		if (solution.size() != other.size()) {
			throw std::invalid_argument("The two solutions must have the same size");
		}
		double distance = 0;
		for (std::size_t i = 1; i < solution.size(); ++i) {
			distance += solution[i] != other[i];
		}
		return distance / solution.size();
	}

	// optimized hamming distance
	double opt_hamming_distance(const Solution& other) const
	{
		if (solution.size() != other.size()) {
			throw std::invalid_argument("The two solutions must have the same size");
		}

		double distance = 0;

		for (size_t i = 0; i < solution.size(); ++i) {
			unsigned xor_result = solution[i] ^ other[i];

// Count the number of set bits
// Use compiler-specific intrinsics if available
#if defined(__GNUC__) || defined(__clang__)
			// printf("Using __builtin_popcount\n");
			distance += __builtin_popcount(xor_result);
#elif defined(_MSC_VER)
			printf("Using __popcnt\n");
			distance += __popcnt(xor_result);
#else
			// Generic implementation
			printf("Using generic implementation\n");
			while (xor_result) {
				distance += xor_result & 1;
				xor_result >>= 1;
			}
#endif
		}

		return distance / solution.size();
	}

  private:
	std::vector<unsigned> solution;
	// std::vector<unsigned> unsatisfying_variables; // Vector that maps unsatisfying variables to the number of clauses
	// they do not satisfy
	int fitness;			 // Number of clauses that are not satisfied by the solution
	double combined_fitness; // Combined fitness of the solution

	float mutation_rate;
	float crossover_rate;
};

// class Population
// {
//   public:
// 	Population(int population_size_)
// 		: population_size(population_size_)
// 	{
// 		population.reserve(population_size_);
// 	}
// 	Population(const Population& other)
// 		: population(other.population)
// 		, population_size(other.population_size)
// 	{
// 	}
// 	Population(int population_size_, int solution_size, int nclauses)
// 		: population(population_size_)
// 		, population_size(population_size_)
// 	{
// 		population.reserve(population_size_);
// 		for (int i = 0; i < population_size_; ++i) {
// 			population.emplace_back(solution_size, nclauses);
// 		}
// 	}
// 	~Population() {}

// 	// Operators overloading
// 	Population& operator=(const Population& other)
// 	{
// 		if (this != &other) {
// 			population = other.population;
// 			population_size = other.population_size;
// 		}

// 		return *this;
// 	}

// 	bool operator==(const Population& p) const { return population == p.population; }

// 	bool operator!=(const Population& p) const { return !(*this == p); }

// 	const Solution& operator[](std::size_t i) const { return population[i]; }
// 	Solution& operator[](std::size_t i) { return population[i]; }

// 	// Getters and setters
// 	std::size_t size() const { return population.size(); }
// 	void resize(std::size_t size_) { population.resize(size_); }
// 	void setPopulation(std::vector<Solution> population_)
// 	{
// 		population.clear();
// 		population.shrink_to_fit();
// 		population = population_;
// 	}
// 	std::vector<Solution> getPopulation() const { return population; }
// 	void push_back(Solution solution) { population.push_back(std::move(solution)); }

// 	void sort()
// 	{
// 		std::sort(population.begin(), population.end(), [](const Solution& a, const Solution& b) {
// 			return a.getFitness() < b.getFitness();
// 		});
// 	}
// 	std::string toString() const
// 	{
// 		std::ostringstream oss;
// 		for (const auto& sol : population) {
// 			oss << sol.toString() << "\t fitness = " << sol.getFitness() << std::endl;
// 		}
// 		return oss.str();
// 	}

//   private:
// 	int population_size;			  // Number of solutions per generation
// 	std::vector<Solution> population; // Population of solutions
// };

class Population
{
  public:
	Population(int population_size_)
		: population_size(population_size_)
	{
		population.reserve(population_size_);
	}
	Population(const Population& other)
		: population(other.population)
		, population_size(other.population_size)
	{
	}
	// Population(const Population &other) = default;
	// Population(Population &&other) noexcept = default;

	Population(int population_size_, int solution_size, int nclauses)
		: population(population_size_)
		, population_size(population_size_)
	{
		population.reserve(population_size_);
		for (int i = 0; i < population_size_; ++i) {
			population.emplace_back(solution_size, nclauses);
		}
	}
	~Population() {}

	// // Operators overloading
	Population& operator=(const Population& other)
	{
		if (this != &other) {
			population = other.population;
			population_size = other.population_size;
		}

		return *this;
	}

	bool operator==(const Population& p) const { return population == p.population; }

	bool operator!=(const Population& p) const { return !(*this == p); }

	const Solution& operator[](std::size_t i) const { return population[i]; }
	Solution& operator[](std::size_t i) { return population[i]; }

	// Getters and setters
	std::size_t size() const { return population.size(); }
	void resize(std::size_t size_) { population.resize(size_); }
	void setPopulation(std::vector<Solution> population_) { population = std::move(population_); }
	std::vector<Solution>& getPopulation() { return population; }
	void push_back(Solution solution) { population.push_back(std::move(solution)); }

	void sort()
	{
		std::sort(population.begin(), population.end(), [](const Solution& a, const Solution& b) {
			return a.getFitness() < b.getFitness();
		});
	}
	void sort_combined()
	{
		std::sort(population.begin(), population.end(), [](const Solution& a, const Solution& b) {
			return a.getCombinedFitness() > b.getCombinedFitness();
		});
	}
	std::string toString() const
	{
		std::ostringstream oss;
		for (const auto& sol : population) {
			oss << sol.toString() << std::endl
				<< "c |  Number of UNSAT clauses = " << sol.getFitness() << std::endl
				<< "c |  Combined fitness = " << sol.getCombinedFitness() << std::endl;
		}
		return oss.str();
	}

  private:
	int population_size;			  // Number of solutions per generation
	std::vector<Solution> population; // Population of solutions
};

class GeneticAlgorithm
{
  public:
	GeneticAlgorithm() = default;

	GeneticAlgorithm(int population_size,
					 int solution_size,
					 int max_iterations,
					 float mutation_rate,
					 float crossover_rate,
					 size_t seed,
					 //  Formula& formula,
					 unsigned int clause_count,
					 unsigned int variable_count,
					 std::vector<simpleClause>& clauses)
		: population_size_(population_size)
		, solution_size_(solution_size + 1)
		, max_iterations_(max_iterations)
		, mutation_rate_(mutation_rate)
		, crossover_rate_(crossover_rate)
		, population_(population_size_)
		, seed(seed)
		// , formula_(formula)
		, clause_count_(clause_count)
		, variable_count_(variable_count)
		, fixed_variables_(variable_count, 0)
		, clauses_(clauses)
	{
		population_ = Population(population_size_);
	}

	~GeneticAlgorithm() {}

	// Operators overloading
	GeneticAlgorithm& operator=(const GeneticAlgorithm& other)
	{
		if (this != &other) {
			population_size_ = other.population_size_;
			solution_size_ = other.solution_size_;
			max_iterations_ = other.max_iterations_;
			mutation_rate_ = other.mutation_rate_;
			crossover_rate_ = other.crossover_rate_;
			population_ = other.population_;
			fixed_variables_ = other.fixed_variables_;
			// formula_ = other.formula_;
			// memo = other.memo;
		}

		return *this;
	}

	Solution solve()
	{

		// Create a random number generator with a fixed seed for reproducibility
		std::random_device rd;
		std::mt19937 rng(seed);
		// std::cout << "c |  Initializing the population ..." << std::endl;
		initialize_population(rng);

		// evaluate_fitness();
		evaluate_fitness_with_diversity(0.50, 0.50);
		// int best_fitness = getBestSolutionByFitness().getFitness();
		Solution best_solution = getBestSolutionByCombinedFitness();
		double best_combined_fitness = best_solution.getCombinedFitness();
		int no_improvement_count = 0;
		const int max_no_improvement_iterations = 200; // You can adjust this threshold

		// TORETHINK: globalEnding or isInterrupted bool to be set => need a SequentialWorker
		for (int iteration = 0; iteration < max_iterations_ && !globalEnding.load(); ++iteration) {
			std::vector<Solution> parents = select_parents_tournament(rng);

			std::vector<Solution> offspring = create_offspring(parents, rng);

			// evaluate_fitness(offspring);
			evaluate_fitness_with_diversity(offspring, 0.50, 0.50);
			select_survivors_ellitist(offspring);

			// Clear the parents and offspring vectors
			parents.clear();
			offspring.clear();
			parents.shrink_to_fit();
			offspring.shrink_to_fit();

			// Check if a solution has been found
			if (solution_found())
				break;

			// Check for improvement
			Solution current_best_solution = getBestSolutionByCombinedFitness();
			double current_combined_fitness = current_best_solution.getCombinedFitness();
			if (current_combined_fitness > best_combined_fitness) {
				best_combined_fitness = current_combined_fitness;
				best_solution = current_best_solution;
				no_improvement_count = 0; // Reset the counter
			} else {
				no_improvement_count++;
			}
			// Check if the maximum number of iterations without improvement has been reached
			if (no_improvement_count >= max_no_improvement_iterations) {
				std::cout << "c |  Stopping early due to no improvement." << std::endl;
				break;
			}

			// if (iteration % 10 == 0)
			// {
			//     std::cout << "c |  Iteration " << iteration << std::endl
			//               << "c |  Best fitness: " << population_[0].getFitness() << std::endl
			//               << "c |  Worst fitness: " << population_.getPopulation().back().getFitness() << std::endl;
			//     std::cout << "c |  \t ==================================== " << std::endl;
			// }
		}
		// Sort the population by fitness in descending order (best solutions first)
		population_.sort_combined();
		return population_[0];
	}

	Solution& getBestSolution();

	// 0 for best, populationSize - 1 for worst
	Solution& getNthSolution(unsigned int n)
	{
		assert(n >= 0 && n < population_size_);
		if (n >= population_size_) {
			LOGWARN("Cannot fetch %u best solution, max is %u. Returning the worst solution");
			return population_[population_size_ - 1];
		}
		return population_[n];
	}
	Solution& getWorstSolution();

	void fixVariable(unsigned int var, char val) { fixed_variables_[var - 1] = val; }

	bool isVariableFixed(unsigned int var) { return fixed_variables_[var] != 0; }

	char getFixedValue(unsigned int var) { return fixed_variables_[var - 1]; }

	Solution& getBestSolutionByFitness();
	Solution& getBestSolutionByCombinedFitness();

	Solution& getWorstSolutionByFitness();
	Solution& getWorstSolutionByCombinedFitness();

  private:
	boost::compute::detail::lru_cache<std::vector<unsigned>, int> memo{
		500
	}; // cache memory to store the results of previous calls to fitness_unsat
	size_t population_size_;
	size_t solution_size_;
	int max_iterations_;
	float mutation_rate_;
	float crossover_rate_;
	Population population_;
	// Formula formula_
	unsigned int clause_count_;
	unsigned int variable_count_;

	std::vector<char> fixed_variables_;
	const std::vector<simpleClause>& clauses_;
	std::vector<unsigned> centrality_vars;

	size_t seed;

	void initialize_population(std::mt19937 rng);
	void evaluate_fitness(std::vector<Solution>& offspring);
	void evaluate_fitness();
	int fitness(Solution& solution);
	int fitness_unsat(Solution& solution);

	Solution select_parent(std::mt19937 rng);
	std::vector<Solution> create_offspring_two_points(std::mt19937 rng);
	std::vector<Solution> create_offspring_three_points(std::mt19937 rng);
	std::vector<Solution> create_offspring(std::mt19937 rng);
	void mutate_unsat(Solution& solution, std::mt19937 rng);
	void mutate_degree_vars(Solution& solution, std::mt19937 rng);
	std::vector<Solution> select_parents_tournament(std::mt19937 rng);
	std::vector<Solution> select_parents_random(std::mt19937 rng);
	std::vector<Solution> create_offspring(const std::vector<Solution>& parents, std::mt19937 rng);
	std::vector<Solution> create_offspring_two_points(const std::vector<Solution>& parents, std::mt19937 rng);
	std::vector<Solution> create_offspring_three_points(const std::vector<Solution>& parents, std::mt19937 rng);
	void select_survivors(const std::vector<Solution>& offspring);
	void select_survivors_ellitist(const std::vector<Solution>& offspring);
	bool solution_found();

	void analyzePopulationDiversity(const Population& population);
	// double compute_solution_fitness_diversity_ratio(const Solution &solution, const Population &population, bool
	// use_optimized = true, double fitness_weight = 0.7, double diversity_weight = 0.3);
	double fitness_with_diversity(Solution& solution, double alpha = 0.7, double beta = 0.3);
	// Evaluation of the fitness of the population with diversity
	void evaluate_fitness_with_diversity(double alpha, double beta);

	// Evaluate fitness with diversity for a set of offspring
	void evaluate_fitness_with_diversity(std::vector<Solution>& offspring, double alpha, double beta);
};
}

#endif // __SAGA_H__
