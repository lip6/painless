#include "GaspiInitializer.hpp"
#include <algorithm>
#include <iostream>
#include <random>

using namespace saga;

#define SIGN(LIT) ((LIT) < 0 ? false : true)

// Initialize the population with random solutions
void
GeneticAlgorithm::initialize_population(std::mt19937 rng)
{

	// Create a uniform distribution for integers in [0, 1]
	std::uniform_int_distribution<int> dist(0, 1);

	for (std::size_t i = 0; i < population_size_; i++) {
		// Create a new solution with random values in its vector
		Solution sol(variable_count_, clause_count_);
		assert(sol.getSolution().size() == variable_count_);
		for (std::size_t j = 0; j < variable_count_; ++j) {
			if (isVariableFixed(j))
				sol[j] = getFixedValue(j) > 0;
			else
				sol[j] = dist(rng); // generate a random bit using the distribution and the generator
		}
		population_.push_back(sol); // Add the solution to the population
	}
}

int
GeneticAlgorithm::fitness_unsat(Solution& solution)
{
	if (memo.contains(solution.getSolution())) {
		// std::cout << "memo hit" << std::endl;
		int fit = memo.get(solution.getSolution()).get_value_or(clause_count_);
		return fit;
	} else {
		assert(clauses_.size() > 0);

		size_t fitness = 0;
		// std::vector<unsigned> unsat_vars;

		for (const auto& clause : clauses_) {
			bool satisfied = false;
			for (int lit : clause) {
				if (!satisfied && solution[std::abs(lit)] == SIGN(lit)) {
					satisfied = true;
				}
			}
			if (!satisfied) {
				fitness++;
				// add all unsat variables to the solution
			}
		}

		memo.insert(solution.getSolution(), fitness);
		return fitness;
	}
}

// Evaluate the fitness of each solution in the population
void
GeneticAlgorithm::evaluate_fitness()
{
	for (std::size_t i = 0; i < population_size_; ++i) {
		assert(population_[i].size() == variable_count_ + 1);
		assert(i < population_.getPopulation().size());
		// Evaluate the fitness of each solution using the fitness function
		// std::cout << "fitness = " << population_.population[i].fitness << std::endl;
		population_[i].setFitness(fitness_unsat(population_[i]));
	}
	// Sort the population by fitness in descending order (best solutions first)
	// std::sort(population_.getPopulation().begin(), population_.getPopulation().end(), [](const Solution &a, const
	// Solution &b)
	//           { return a.getFitness() < b.getFitness(); });
	population_.sort();
	// std::cout << "best fitness = " << population_.population[0].fitness << std::endl;
}
// Evaluate the fitness of the offspring
void
GeneticAlgorithm::evaluate_fitness(std::vector<Solution>& offspring)
{
	for (auto& sol : offspring) {
		sol.setFitness(fitness_unsat(sol));
	}
}

// Select parents using tournament selection
std::vector<Solution>
GeneticAlgorithm::select_parents_tournament(std::mt19937 rng)
{
	// Create a vector to store the selected parents
	std::vector<Solution> parents;
	parents.reserve(population_size_ / 2);
	// Create a uniform distribution for integers in [0, population_size_ - 1]
	std::uniform_int_distribution<int> dist(0, population_size_ - 1);

	for (std::size_t i = 0; i < (population_size_ / 2); ++i) {
		// Select two random solutions from the population as candidates for reproduction
		int index1 = dist(rng);
		int index2 = dist(rng);
		Solution& candidate1 = population_[index1];
		Solution& candidate2 = population_[index2];

		// Compare their fitness and select the fitter one as a parent
		if (candidate1.getFitness() < candidate2.getFitness()) {
			parents.push_back(std::move(candidate1));
		} else {
			parents.push_back(std::move(candidate2));
		}
	}

	return parents;
}

std::vector<Solution>
GeneticAlgorithm::select_parents_random(std::mt19937 rng)
{
	std::vector<Solution> parents;
	parents.reserve(population_size_ / 2);
	// Create a uniform distribution for integers in [0, population_size_ - 1]
	std::uniform_int_distribution<int> dist(0, population_size_ - 1);
	for (int i = 0; i < (population_size_ / 2); ++i) {
		int index = dist(rng);
		parents.push_back(std::move(population_[index]));
	}
	return parents;
}

void
GeneticAlgorithm::mutate_degree_vars(Solution& solution, std::mt19937 rng)
{
	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	for (auto& var : centrality_vars) {

		if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
			solution[var] = 1 - solution[var];
		}
	}
}

// Create offspring through crossover and mutation
std::vector<Solution>
GeneticAlgorithm::create_offspring(const std::vector<Solution>& parents, std::mt19937 rng)
{
	// Create a vector to store the offspring solutions
	std::vector<Solution> offspring;
	offspring.reserve(parents.size());

	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	// Create a uniform distribution for integers in [1, solution_size_ - 2]
	std::uniform_int_distribution<int> dist2(1, solution_size_ - 2);
	// Create a uniform distribution for integers in [0, parents.size() - 1]
	std::uniform_int_distribution<int> dist3(0, parents.size() - 1);

	for (std::size_t i = 0; i < parents.size() - 1; i += 2) {
		// Select two parents from the vector using their index
		Solution parent1 = parents[i];
		Solution parent2 = parents[i + 1];
		// Solution parent1 = parents[dist3(rng)];
		// Solution parent2 = parents[dist3(rng)];

		Solution child1(parent1);
		Solution child2(parent2);

		// Perform crossover with a given probability
		if (dist(rng) < crossover_rate_) {
			// Select a random point to split the solution vector
			int point = dist2(rng);
			// std::cout << "point: " << point << std::endl;

			for (int j = 1; j <= point; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
		}

		for (auto& var : centrality_vars) {
			// int unsat_var = var.first;
			// assert(!isVariableFixed(var));
			if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
				child1[var] = 1 - child1[var];
				child2[var] = 1 - child1[var];
			}
		}

		// Add the offspring to the vector
		offspring.push_back(std::move(child1));
		offspring.push_back(std::move(child2));
	}

	return offspring;
}

Solution
GeneticAlgorithm::select_parent(std::mt19937 rng)
{
	// Create a uniform distribution for integers in [0, population_size_ - 1]
	std::uniform_int_distribution<int> dist(0, population_size_ - 1);
	int index = dist(rng);
	return population_[index];
}

std::vector<Solution>
GeneticAlgorithm::create_offspring(std::mt19937 rng)
{
	// Create a vector to store the offspring solutions
	std::vector<Solution> offspring;
	offspring.reserve(population_.size());

	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	// Create a uniform distribution for integers in [1, solution_size_ - 2]
	std::uniform_int_distribution<int> dist2(1, solution_size_ - 2);

	for (std::size_t i = 0; i < population_.size() / 2; i += 2) {
		// Select two parents from the vector using their index
		Solution parent1 = select_parent(rng);
		Solution parent2 = select_parent(rng);

		Solution child1(parent1);
		Solution child2(parent2);

		// Perform crossover with a given probability
		if (dist(rng) < crossover_rate_) {
			// Select a random point to split the solution vector
			int point = dist2(rng);
			// std::cout << "point: " << point << std::endl;

			for (int j = 0; j <= point; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
		}

		for (auto& var : centrality_vars) {
			// int unsat_var = var.first;
			// assert(!isVariableFixed(var));
			if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
				child1[var] = 1 - child1[var];
				child2[var] = 1 - child1[var];
			}
		}

		// Add the offspring to the vector
		offspring.push_back(std::move(child1));
		offspring.push_back(std::move(child2));
	}

	return offspring;
}

std::vector<Solution>
GeneticAlgorithm::create_offspring_two_points(std::mt19937 rng)
{
	// Create a vector to store the offspring solutions
	std::vector<Solution> offspring;
	offspring.reserve(population_.size());

	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	// Create a uniform distribution for integers in [1, solution_size_ - 2]
	std::uniform_int_distribution<int> dist2(1, solution_size_ - 2);

	for (size_t i = 0; i < population_.size() / 2; i++) {
		// Select two parents from the vector using their index
		Solution parent1 = select_parent(rng);
		Solution parent2 = select_parent(rng);

		Solution child1(parent1);
		Solution child2(parent2);

		// Perform crossover with a given probability
		if (dist(rng) < crossover_rate_) {
			// Select a random point to split the solution vector
			int point1 = dist2(rng);
			int point2 = dist2(rng);
			if (point1 > point2) {
				std::swap(point1, point2);
			}
			for (int j = 0; j <= point1; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
			for (int j = point2; j < solution_size_; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
		}

		for (auto& var : centrality_vars) {
			// int unsat_var = var.first;
			// assert(!isVariableFixed(var));
			if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
				child1[var] = 1 - child1[var];
				child2[var] = 1 - child1[var];
			}
		}

		// Add the offspring to the vector
		offspring.push_back(std::move(child1));
		offspring.push_back(std::move(child2));
	}
	return offspring;
}

// Create offspring through two points crossover and mutation
std::vector<Solution>
GeneticAlgorithm::create_offspring_two_points(const std::vector<Solution>& parents, std::mt19937 rng)
{
	// Create a vector to store the offspring solutions
	std::vector<Solution> offspring;
	offspring.reserve(parents.size());

	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	// Create a uniform distribution for integers in [1, solution_size_ - 2]
	std::uniform_int_distribution<int> dist2(1, solution_size_ - 2);
	// Create a uniform distribution for integers in [0, parents.size() - 1]
	std::uniform_int_distribution<int> dist3(0, parents.size() - 1);

	for (std::size_t i = 0; i < parents.size() - 1; i += 2) {
		// Select two parents from the vector using their index
		Solution parent1 = parents[i];
		Solution parent2 = parents[i + 1];
		// Solution parent1 = parents[dist3(rng)];
		// Solution parent2 = parents[dist3(rng)];

		Solution child1(parent1);
		Solution child2(parent2);

		// Perform crossover with a given probability
		if (dist(rng) < crossover_rate_) {
			// Select a random point to split the solution vector
			int point1 = dist2(rng);
			int point2 = dist2(rng);
			if (point1 > point2) {
				std::swap(point1, point2);
			}
			for (int j = 0; j <= point1; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
			for (int j = point2; j < solution_size_; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
		}

		for (auto& var : centrality_vars) {
			// int unsat_var = var.first;
			// assert(!isVariableFixed(var));
			if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
				child1[var] = 1 - child1[var];
				child2[var] = 1 - child1[var];
			}
		}

		// Add the offspring to the vector
		offspring.push_back(std::move(child1));
		offspring.push_back(std::move(child2));
	}

	return offspring;
}

// Create offspring through three points crossover and mutation
std::vector<Solution>
GeneticAlgorithm::create_offspring_three_points(const std::vector<Solution>& parents, std::mt19937 rng)
{
	// Create a vector to store the offspring solutions
	std::vector<Solution> offspring;
	offspring.reserve(parents.size());

	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	// Create a uniform distribution for integers in [1, solution_size_ - 2]
	std::uniform_int_distribution<int> dist2(1, solution_size_ - 2);
	// Create a uniform distribution for integers in [0, parents.size() - 1]
	std::uniform_int_distribution<int> dist3(0, parents.size() - 1);

	for (std::size_t i = 0; i < parents.size() - 1; i += 2) {
		// Select two parents from the vector using their index
		Solution parent1 = parents[i];
		Solution parent2 = parents[i + 1];
		// Solution parent1 = parents[dist3(rng)];
		// Solution parent2 = parents[dist3(rng)];

		Solution child1(parent1);
		Solution child2(parent2);

		// Perform crossover with a given probability
		if (dist(rng) < crossover_rate_) {
			// Select a random point to split the solution vector
			int point1 = dist2(rng);
			int point2 = dist2(rng);
			int point3 = dist2(rng);

			// sort the points
			if (point1 > point2) {
				std::swap(point1, point2);
			}
			if (point2 > point3) {
				std::swap(point2, point3);
			}
			if (point1 > point2) {
				std::swap(point1, point2);
			}

			for (int j = 0; j <= point1; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
			for (int j = point2; j <= point3; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
		}

		for (auto& var : centrality_vars) {
			// int unsat_var = var.first;
			// assert(!isVariableFixed(var));
			if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
				child1[var] = 1 - child1[var];
				child2[var] = 1 - child1[var];
			}
		}

		// Add the offspring to the vector
		offspring.push_back(std::move(child1));
		offspring.push_back(std::move(child2));
	}

	return offspring;
}

// Create offspring through three points crossover and mutation
std::vector<Solution>
GeneticAlgorithm::create_offspring_three_points(std::mt19937 rng)
{
	// Create a vector to store the offspring solutions
	std::vector<Solution> offspring;
	offspring.reserve(population_.size());

	// Create a uniform distribution for floats in [0.0, 1.0]
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);
	// Create a uniform distribution for integers in [1, solution_size_ - 2]
	std::uniform_int_distribution<int> dist2(1, solution_size_ - 2);

	for (std::size_t i = 0; i < population_.size() / 2; i += 2) {
		// Select two parents from the vector using their index
		Solution parent1 = select_parent(rng);
		Solution parent2 = select_parent(rng);
		Solution child1(parent1);
		Solution child2(parent2);

		// Perform crossover with a given probability
		if (dist(rng) < crossover_rate_) {
			// Select a random point to split the solution vector
			int point1 = dist2(rng);
			int point2 = dist2(rng);
			int point3 = dist2(rng);

			// sort the points
			if (point1 > point2) {
				std::swap(point1, point2);
			}
			if (point2 > point3) {
				std::swap(point2, point3);
			}
			if (point1 > point2) {
				std::swap(point1, point2);
			}

			for (int j = 0; j <= point1; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
			for (int j = point2; j <= point3; ++j) {
				if (!isVariableFixed(j)) {
					child1[j] = parent2[j];
					child2[j] = parent1[j];
				}
			}
		}

		for (auto& var : centrality_vars) {
			// int unsat_var = var.first;
			// assert(!isVariableFixed(var));
			if (dist(rng) < mutation_rate_ && !isVariableFixed(var)) {
				child1[var] = 1 - child1[var];
				child2[var] = 1 - child1[var];
			}
		}

		// Add the offspring to the vector
		offspring.push_back(std::move(child1));
		offspring.push_back(std::move(child2));
	}

	return offspring;
}

// Select the best solutions to survive to the next generation
void
GeneticAlgorithm::select_survivors(const std::vector<Solution>& offspring)
{
	// Combine the population and offspring
	std::vector<Solution> combined(population_.getPopulation());
	// combined.resize(combined.size() + offspring.size());
	combined.insert(combined.end(), offspring.begin(), offspring.end());
	// std::cout << "combined size before: " << combined.size() << std::endl;
	// Select the best solutions to survive
	size_t erasureCount = 0;
	size_t maxErasures = combined.size() - population_size_;
	if (maxErasures > 0) // Maximum allowed erasures
		combined.erase(unique(combined.begin(),
							  combined.end(),
							  [&erasureCount, maxErasures](const Solution& s1,
														   const Solution& s2) { // Lambda function to compare solutions
								  if (erasureCount < maxErasures) {
									  ++erasureCount;
									  return s1 == s2;
								  } else {
									  return false;
								  }
							  }),
					   combined.end());

	// Sort the combined vector by fitness in ascending order
	std::sort(combined.begin(), combined.end(), [](const Solution& s1, const Solution& s2) {
		return s1.getFitness() < s2.getFitness();
	});

	population_.setPopulation(std::vector<Solution>(combined.begin(), combined.begin() + population_size_));
	assert(population_.getPopulation().size() == population_size_);
	combined.clear();
	combined.shrink_to_fit();
}

// Select the best solutions to survive to the next generation
void
GeneticAlgorithm::select_survivors_ellitist(const std::vector<Solution>& offspring)
{
	// Combine the population and offspring
	std::vector<Solution> combined(population_.getPopulation());
	// combined.resize(combined.size() + offspring.size());
	combined.insert(combined.end(), offspring.begin(), offspring.end());
	// std::cout << "combined size before: " << combined.size() << std::endl;
	// Select the best solutions to survive
	size_t erasureCount = 0;
	size_t maxErasures = combined.size() - population_size_;
	if (maxErasures > 0) // Maximum allowed erasures
		combined.erase(unique(combined.begin(),
							  combined.end(),
							  [&erasureCount, maxErasures](const Solution& s1,
														   const Solution& s2) { // Lambda function to compare solutions
								  if (erasureCount < maxErasures) {
									  ++erasureCount;
									  return s1 == s2;
								  } else {
									  return false;
								  }
							  }),
					   combined.end());

	// Sort the combined vector by fitness in ascending order
	std::sort(combined.begin(), combined.end(), [](const Solution& s1, const Solution& s2) {
		return s1.getFitness() < s2.getFitness();
	});

	// Select the 50% best and 50% worst solutions to survive
	size_t nbest = population_size_ / 2;
	size_t nworst = population_size_ - nbest;

	std::vector<Solution> survivors;
	survivors.reserve(population_size_);
	survivors.insert(survivors.end(), combined.begin(), combined.begin() + nbest);
	// std::random_device rd;
	std::mt19937 g(seed);
	std::shuffle(combined.begin() + nbest, combined.end(), g);
	survivors.insert(survivors.end(), combined.begin() + nbest, combined.begin() + nbest + nworst);
	// survivors.insert(survivors.end(), combined.end() - nworst, combined.end());
	population_.setPopulation(survivors);
	assert(population_.getPopulation().size() == population_size_);
	combined.clear();
	combined.shrink_to_fit();
	survivors.clear();
	survivors.shrink_to_fit();
}

// Get the fittest solution
Solution&
GeneticAlgorithm::getBestSolution()
{
	return population_[0];
}

// Get the worst solution
Solution&
GeneticAlgorithm::getWorstSolution()
{
	return population_[population_size_ - 1];
}

// Check if a satisfactory solution has been found
bool
GeneticAlgorithm::solution_found()
{
	// Check if the fittest solution satisfies all the clauses
	Solution& fittest = getBestSolutionByFitness();
	return fittest.getFitness() == 0;
}

// Get the fittest solution
Solution&
GeneticAlgorithm::getBestSolutionByFitness()
{
	auto it = std::min_element(population_.getPopulation().begin(),
							   population_.getPopulation().end(),
							   [](const Solution& a, const Solution& b) { return a.getFitness() < b.getFitness(); });

	return *it;
}

// Get the worst solution
Solution&
GeneticAlgorithm::getWorstSolutionByFitness()
{
	auto it = std::max_element(population_.getPopulation().begin(),
							   population_.getPopulation().end(),
							   [](const Solution& a, const Solution& b) { return a.getFitness() < b.getFitness(); });

	return *it;
}

Solution&
GeneticAlgorithm::getBestSolutionByCombinedFitness()
{
	population_.sort_combined();
	return population_[0];
}
Solution&
GeneticAlgorithm::getWorstSolutionByCombinedFitness()
{
	auto it = std::min_element(
		population_.getPopulation().begin(),
		population_.getPopulation().end(),
		[](const Solution& a, const Solution& b) { return a.getCombinedFitness() < b.getCombinedFitness(); });

	return *it;
}

// Function to compute average Hamming distance for one solution against all others in the population
double
compute_average_distance(const Solution& solution, const Population& population, bool use_optimized = true)
{
	double total_distance = 0.0;
	int comparisons = 0;

	// Compare with all other solutions in the population
	for (size_t j = 0; j < population.size(); ++j) {
		// Skip comparing with itself if the solution is part of the population
		if (&solution == &population[j])
			continue;

		double distance = 0.0;
		try {
			if (use_optimized) {
				distance = solution.opt_hamming_distance(population[j]);
			} else {
				distance = solution.hamming_distance(population[j]);
			}
			total_distance += distance;
			comparisons++;
		} catch (const std::invalid_argument& e) {
			std::cerr << "Error comparing solutions: " << e.what() << std::endl;
		}
	}

	// Calculate average
	return (comparisons > 0) ? total_distance / comparisons : 0.0;
}

// Function to compute average Hamming distances for all individuals in the population
std::vector<double>
compute_population_distances(const Population& population, bool use_optimized = true)
{
	const size_t pop_size = population.size();
	std::vector<double> avg_distances(pop_size, 0.0);

	// For each solution in the population
	for (size_t i = 0; i < pop_size; ++i) {
		avg_distances[i] = compute_average_distance(population[i], population, use_optimized);
	}

	return avg_distances;
}

// Function to compute overall diversity of the population
double
compute_population_diversity(const Population& population, bool use_optimized = true)
{
	const std::vector<double> avg_distances = compute_population_distances(population, use_optimized);

	// Return average of all average distances
	double sum = 0.0;
	for (const double& dist : avg_distances) {
		sum += dist;
	}

	return population.size() > 0 ? sum / population.size() : 0.0;
}

// Function to analyze the diversity of the population
void
GeneticAlgorithm::analyzePopulationDiversity(const Population& population)
{
	// Example of using the individual function for a specific solution
	if (!population.size() == 0) {
		double diversity_of_first = compute_average_distance(population[0], population, true);
		std::cout << "Diversity of first solution: " << diversity_of_first << std::endl;
		std::cout << "Population diversity: " << compute_population_diversity(population, true) << std::endl;
	}

	// Example of computing distances for all solutions
	std::vector<double> distances = compute_population_distances(population, true);

	// Print individual diversity scores
	std::cout << "Individual diversity scores:" << std::endl;
	for (size_t i = 0; i < distances.size(); ++i) {
		std::cout << "Solution " << i << ": " << distances[i] << std::endl;
	}

	// Print individual fitness scores
	std::cout << "Individual fitness scores:" << std::endl;
	for (size_t i = 0; i < distances.size(); ++i) {
		std::cout << "Solution " << i << ": " << population[i].getFitness() << std::endl;
	}
}

// Evaluate the fitness of one solution in the population with diversity
double
GeneticAlgorithm::fitness_with_diversity(Solution& solution, double alpha, double beta)
{
	// Get the base fitness (unsat clauses count)
	int unsat_count = fitness_unsat(solution);
	// std::cout << "          unsat_count = " << unsat_count << std::endl;
	solution.setFitness(unsat_count);
	// Count the number of satisfied clauses
	int sat_count = clause_count_ - unsat_count;
	// std::cout << "          sat_count = " << sat_count << std::endl;

	// Compute diversity component (avg Hamming distance to population)
	double avg_distance = compute_average_distance(solution, population_, true);

	// Normalize both metrics
	double norm_sat = (sat_count < clause_count_) ? static_cast<double>(sat_count) / clause_count_ : 1.0;
	// avg_distance is already normalized (by design of the Hamming distance function)

	// Parameters to control the balance (can be tuned)
	// const double alpha = 0.7; // Weight for satisfiability
	// const double beta = 0.3;  // Weight for diversity

	// Combined fitness (lower is better)
	double combined_fitness = alpha * norm_sat + beta * avg_distance;
	solution.setCombinedFitness(combined_fitness);

	// Convert back to same scale as original fitness for compatibility
	return combined_fitness;
}

// Evaluate the fitness of each solution in the population with diversity
void
GeneticAlgorithm::evaluate_fitness_with_diversity(double alpha, double beta)
{
	for (size_t i = 0; i < population_.size(); ++i) {
		assert(i < population_.size());
		// std::cout << "  population[" << i << "] size = " << population_[i].size() << std::endl;
		assert(population_[i].size() == variable_count_);
		double combined_fitness = fitness_with_diversity(population_[i], alpha, beta);

		// Set the fitness value
		population_[i].setCombinedFitness(combined_fitness);
	}

	try {

		std::vector<Solution> pop = population_.getPopulation();
		std::sort(pop.begin(), pop.end(), [](const Solution& a, const Solution& b) {
			return a.getCombinedFitness() > b.getCombinedFitness();
		});
		population_.setPopulation(pop);

	} catch (const std::exception& e) {
		std::cerr << "Exception during sort: " << e.what() << std::endl;
	}
}

// Evaluate fitness with diversity for a set of offspring
void
GeneticAlgorithm::evaluate_fitness_with_diversity(std::vector<Solution>& offspring, double alpha, double beta)
{
	// Evaluate each offspring individually
	for (auto& sol : offspring) {
		double combined_fitness = fitness_with_diversity(sol, alpha, beta);

		// Set the fitness value
		sol.setCombinedFitness(combined_fitness);
	}
}
