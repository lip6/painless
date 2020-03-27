// Copyright 2017 Hakan Metin - LIP6

#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <deque>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>

// #include "sattools/IntegralTypes.h"
// #include "sattools/Logging.h"
// #include "sattools/Macros.h"
// #include "sattools/Printer.h"
// #include "sattools/Timer.h"

// Forward Declaration
class TimeDistribution;

class Stat {
 public:
    explicit Stat(const std::string& name);
    virtual ~Stat() {}

    std::string name() const { return _name;}
    virtual std::string valueString() const = 0;

   //  void print() const {
      //   Printer::printStat(_name, valueString());
   //  }
 private:
    std::string _name;
};

/*----------------------------------------------------------------------------*/
class DistributionStat : public Stat {
 public:
    explicit DistributionStat(const std::string& name);
    ~DistributionStat() override {}

    std::string valueString() const override = 0;

    double  sum() const { return _sum; }
    double  max() const { return _max; }
    double  min() const { return _min; }
    int64_t num() const { return _num; }
    double  average()      const;
    double stdDeviation() const;
 protected:
    void addToDistribution(double value);
    double  _sum;
    double  _average;
    double  _sum_squares_from_average;
    double  _min;
    double  _max;
    int64_t _num;
};

/*---------------------------------------------------------------------------*/
// Statistic on the distribution of a sequence of integers.
class IntegerDistribution : public DistributionStat {
 public:
    explicit IntegerDistribution(const std::string& name)
        : DistributionStat(name) {}
    std::string valueString() const override;
  void add(int64_t value);
};

/*----------------------------------------------------------------------------*/