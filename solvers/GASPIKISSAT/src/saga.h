#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <sstream>
#include <math.h>
#include <utility>
#include <random>
#include <boost/compute/detail/lru_cache.hpp>

#include "kissat.h"

class Formula
{
public:
    Formula() = default;
    Formula(int numVariables, int numClauses)
        : numVariables(numVariables), numClauses(numClauses),
          fix(numVariables + 1, 0),
          fixed_vars(numVariables + 1, 0)
    {
    }

    Formula(const Formula &other) : numVariables(other.numVariables), numClauses(other.numClauses),
                                    fix(other.fix),
                                    fixed_vars(other.fixed_vars),

                                    degree_centrality_variables(other.degree_centrality_variables)
    {
    }

    ~Formula() = default;

    Formula &operator=(const Formula &other)
    {
        numVariables = other.numVariables;
        numClauses = other.numClauses;
        fix = other.fix;
        fixed_vars = other.fixed_vars;
        degree_centrality_variables = other.degree_centrality_variables;
        return *this;
    }

    int getNumVariables() const
    {
        return numVariables;
    }

    int getNumClauses() const
    {
        return numClauses;
    }

    std::vector<bool> &getFix()
    {
        return fix;
    }

    std::vector<bool> &getFixedVars()
    {
        return fixed_vars;
    }

    void setNumVariables(int numVariables)
    {
        this->numVariables = numVariables;
        // this->var_lit_count.resize(numVariables + 1);
        this->fix.resize(numVariables + 1);
        this->fixed_vars.resize(numVariables + 1);
    }

    void setNumClauses(int numClauses)
    {
        this->numClauses = numClauses;
        // this->clauses.resize(numClauses);
        // this->clause_lit_count.resize(numClauses);
        // this->clause_delete.resize(numClauses);
    }

    std::vector<unsigned> &get_degree_centrality_variables()
    {
        return degree_centrality_variables;
    }
    void set_degree_centrality_variables(std::vector<unsigned> &degree_centrality_variables)
    {
        this->degree_centrality_variables = degree_centrality_variables;
    }

    std::vector<std::pair<unsigned, unsigned>> &get_binary_clauses()
    {
        return binary_clauses;
    }
    void set_binary_clauses(std::vector<std::pair<unsigned, unsigned>> &binary_clauses)
    {
        this->binary_clauses = binary_clauses;
    }
    void add_binary_clause(unsigned lit1, unsigned lit2)
    {
        binary_clauses.emplace_back(lit1, lit2);
    }

    std::vector<bool> fix;        // fixed variable.
    std::vector<bool> fixed_vars; // Values of  Fixed variables.

private:
    std::vector<unsigned> degree_centrality_variables;
    std::vector<std::pair<unsigned, unsigned>> binary_clauses;
    int numVariables;
    int numClauses;
};

class Solution
{
public:
    Solution(const Solution &other) : solution(other.solution), fitness(other.fitness) //, unsatisfying_variables(other.unsatisfying_variables)
    {
    }
    Solution(std::vector<unsigned> solution_, int fitness_) : solution(solution_), fitness(fitness_) //, unsatisfying_variables()
    {
    }
    Solution(size_t solution_size, int nclauses)
        : solution(solution_size), fitness(nclauses) //, unsatisfying_variables()
    {
        solution.reserve(solution_size);
    }

    Solution() : solution(), fitness(1000000) {}

    ~Solution() {}

    // Operators overloading
    Solution &operator=(const Solution &other)
    {
        if (this != &other)
        {
            solution = other.solution;
            fitness = other.fitness;
            // unsatisfying_variables = other.unsatisfying_variables;
        }

        return *this;
    }

    bool operator==(const Solution &s) const
    {
        return solution == s.solution && fitness == s.fitness;
    }

    bool operator!=(const Solution &s) const
    {
        return !(*this == s);
    }

    const unsigned &operator[](std::size_t i) const { return solution[i]; }
    unsigned &operator[](std::size_t i)
    {
        return solution[i];
    }

    std::string toString() const
    {
        std::ostringstream oss;
        for (int i = 1; i < size(); ++i)
            oss << (solution[i] ? "" : "-") << i << " ";
        oss << std::endl;
        return oss.str();
    }

    void setRandomSolution(std::mt19937 &rng)
    {
        std::uniform_int_distribution<int> dist(0, 1);
        for (std::size_t i = 1; i < solution.size(); ++i)
        {
            solution[i] = dist(rng);
        }
    }

    // Getters and setters
    int getFitness() const { return fitness; }
    void setFitness(int fitness_) { fitness = fitness_; }
    std::size_t size() const { return solution.size(); }
    void resize(std::size_t size_) { solution.resize(size_); }
    std::vector<unsigned> getSolution() const { return solution; }
    void setSolution(std::vector<unsigned> solution_) { solution = solution_; }
    // std::vector<unsigned> get_unsatisfying_variables() const { return unsatisfying_variables; }

    // bool no_unsatisfying_variables() const
    // {
    //     return unsatisfying_variables.empty();
    // }

    // void add_unsatisfying_variable(int var)
    // {
    //     // Check if the variable already exists in the vector
    //     if (std::find(unsatisfying_variables.begin(), unsatisfying_variables.end(), var) == unsatisfying_variables.end())
    //     {
    //         unsatisfying_variables.push_back(var);
    //     }
    // }

    // int getRandomUnsatisfyingVariable()
    // {
    //     std::random_device rd;
    //     std::mt19937 rng(rd());
    //     // std::cout << "unsat var size: " << unsatisfying_variables.size() << std::endl;
    //     std::uniform_int_distribution<int> dis(0, unsatisfying_variables.size() - 1);
    //     return unsatisfying_variables[dis(rng)].first;
    // }

    // int getMaxUnsatisfyingVariable()
    // {

    //     int max = 0;
    //     int max_var = 0;
    //     for (auto var : unsatisfying_variables)
    //     {
    //         if (var.second > max)
    //         {
    //             max = var.second;
    //             max_var = var.first;
    //         }
    //     }
    //     return max_var;
    // }

private:
    std::vector<unsigned> solution;
    // std::vector<unsigned> unsatisfying_variables; // Vector that maps unsatisfying variables to the number of clauses they do not satisfy
    int fitness;
};

class Population
{
public:
    Population(int population_size_) : population_size(population_size_)
    {
        population.reserve(population_size_);
    }
    Population(const Population &other) : population(other.population), population_size(other.population_size) {}
    Population(int population_size_, int solution_size, int nclauses)
        : population(population_size_), population_size(population_size_)
    {
        population.reserve(population_size_);
        for (int i = 0; i < population_size_; ++i)
        {
            population.emplace_back(solution_size, nclauses);
        }
    }
    ~Population() {}

    // Operators overloading
    Population &operator=(const Population &other)
    {
        if (this != &other)
        {
            population = other.population;
            population_size = other.population_size;
        }

        return *this;
    }

    bool operator==(const Population &p) const
    {
        return population == p.population;
    }

    bool operator!=(const Population &p) const
    {
        return !(*this == p);
    }

    const Solution &operator[](std::size_t i) const { return population[i]; }
    Solution &operator[](std::size_t i) { return population[i]; }

    // Getters and setters
    std::size_t size() const { return population.size(); }
    void resize(std::size_t size_) { population.resize(size_); }
    void setPopulation(std::vector<Solution> population_)
    {
        population.clear();
        population.shrink_to_fit();
        population = population_;
    }
    std::vector<Solution> getPopulation() const { return population; }
    void push_back(Solution solution) { population.push_back(std::move(solution)); }

    void sort()
    {
        std::sort(population.begin(), population.end(), [](const Solution &a, const Solution &b)
                  { return a.getFitness() < b.getFitness(); });
    }
    std::string toString() const
    {
        std::ostringstream oss;
        for (const auto &sol : population)
        {
            oss << sol.toString() << "\t fitness = " << sol.getFitness() << std::endl;
        }
        return oss.str();
    }

private:
    int population_size;              // Number of solutions per generation
    std::vector<Solution> population; // Population of solutions
};

class GeneticAlgorithm
{
public:
    GeneticAlgorithm() = default;
    // GeneticAlgorithm(int population_size, int solution_size, int max_iterations, float mutation_rate,
    //                  float crossover_rate, Formula &formula, kissat *solver)
    //     : population_size_(population_size),
    //       solution_size_(solution_size + 1),
    //       max_iterations_(max_iterations),
    //       mutation_rate_(mutation_rate),
    //       crossover_rate_(crossover_rate),
    //       population_(population_size),
    //       formula_(formula) //,
    //                         // solver_(solver)
    // {
    //     solver = solver;
    //     population_ = Population(population_size_);
    // }
    // GeneticAlgorithm(int population_size, int solution_size, int max_iterations, float mutation_rate,
    //                  float crossover_rate, kissat *solver)
    //     : population_size_(population_size),
    //       solution_size_(solution_size + 1),
    //       max_iterations_(max_iterations),
    //       mutation_rate_(mutation_rate),
    //       crossover_rate_(crossover_rate),
    //       population_(population_size),
    //       formula_(solver->formula),
    //       solver(solver)

    // {
    //     population_ = Population(population_size_);
    //     // solver = solver;
    //     // formula_ = solver->formula;
    // }

    GeneticAlgorithm(int population_size, int solution_size, int max_iterations, float mutation_rate,
                     float crossover_rate, Formula &formula, kissat *solver)
        : population_size_(population_size),
          solution_size_(solution_size + 1),
          max_iterations_(max_iterations),
          mutation_rate_(mutation_rate),
          crossover_rate_(crossover_rate),
          population_(population_size_),
          formula_(formula),
          solver(solver) // Initialize the 'solver' member using the member initializer list
    {
        population_ = Population(population_size_);
    }
    ~GeneticAlgorithm() {}

    // Operators overloading
    GeneticAlgorithm &operator=(const GeneticAlgorithm &other)
    {
        if (this != &other)
        {
            population_size_ = other.population_size_;
            solution_size_ = other.solution_size_;
            max_iterations_ = other.max_iterations_;
            mutation_rate_ = other.mutation_rate_;
            crossover_rate_ = other.crossover_rate_;
            population_ = other.population_;
            formula_ = other.formula_;
            solver = other.solver;
            // memo = other.memo;
        }

        return *this;
    }

    Solution solve()
    {

        // Create a random number generator with a fixed seed for reproducibility
        std::random_device rd;
        std::mt19937 rng(rd());
        // std::cout << "c |  Initializing the population ..." << std::endl;
        initialize_population(rng);

        evaluate_fitness();

        for (int iteration = 0; iteration < max_iterations_; ++iteration)
        {
            std::vector<Solution> parents = select_parents_tournament(rng);

            std::vector<Solution> offspring = create_offspring(parents, rng);

            evaluate_fitness(offspring);
            select_survivors_ellitist(offspring);

            // Clear the parents and offspring vectors
            parents.clear();
            offspring.clear();
            parents.shrink_to_fit();
            offspring.shrink_to_fit();

            // Check if a solution has been found
            if (solution_found())
                break;

            // if (iteration % 10 == 0)
            // {
            //     std::cout << "c |  Iteration " << iteration << std::endl
            //               << "c |  Best fitness: " << population_[0].getFitness() << std::endl
            //               << "c |  Worst fitness: " << population_.getPopulation().back().getFitness() << std::endl;
            //     std::cout << "c |  \t ==================================== " << std::endl;
            // }
        }

        return population_[0];
    }

    Solution &getBestSolution();
    Solution &getWorstSolution();

private:
    boost::compute::detail::lru_cache<std::vector<unsigned>, int> memo{500}; // cache memory to store the results of previous calls to fitness_unsat
    size_t population_size_;
    size_t solution_size_;
    int max_iterations_;
    float mutation_rate_;
    float crossover_rate_;
    Population population_;
    Formula formula_;
    kissat *solver;

    void initialize_population(std::mt19937 rng);
    void evaluate_fitness(std::vector<Solution> &offspring);
    void evaluate_fitness();
    int fitness(Solution &solution);
    int fitness_unsat(Solution &solution);

    Solution select_parent(std::mt19937 rng);
    std::vector<Solution> create_offspring_two_points(std::mt19937 rng);
    std::vector<Solution> create_offspring_three_points(std::mt19937 rng);
    std::vector<Solution> create_offspring(std::mt19937 rng);
    void mutate_unsat(Solution &solution, std::mt19937 rng);
    void mutate_degree_vars(Solution &solution, std::mt19937 rng);
    std::vector<Solution> select_parents_tournament(std::mt19937 rng);
    std::vector<Solution> select_parents_random(std::mt19937 rng);
    std::vector<Solution> create_offspring(const std::vector<Solution> &parents, std::mt19937 rng);
    std::vector<Solution> create_offspring_two_points(const std::vector<Solution> &parents, std::mt19937 rng);
    std::vector<Solution> create_offspring_three_points(const std::vector<Solution> &parents, std::mt19937 rng);
    void select_survivors(const std::vector<Solution> &offspring);
    void select_survivors_ellitist(const std::vector<Solution> &offspring);
    bool solution_found();
};

void initialize_polarity(Solution &solution, kissat *solver);