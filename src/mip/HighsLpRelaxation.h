/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2022 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/*    Authors: Julian Hall, Ivet Galabova, Leona Gottwald and Michael    */
/*    Feldmeier                                                          */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#ifndef HIGHS_LP_RELAXATION_H_
#define HIGHS_LP_RELAXATION_H_

#include <cstdint>
#include <memory>

#include "Highs.h"
#include "mip/HighsMipSolver.h"

class HighsDomain;
struct HighsCutSet;
class HighsPseudocost;

class HighsLpRelaxation {
 public:
  enum class Status {
    kNotSet,
    kOptimal,
    kInfeasible,
    kUnscaledDualFeasible,
    kUnscaledPrimalFeasible,
    kUnscaledInfeasible,
    kUnbounded,
    kError,
  };

 private:
  struct LpRow {
    enum Origin {
      kModel,
      kCutPool,
    };

    Origin origin;
    HighsInt index;
    HighsInt age;

    void get(const HighsMipSolver& mipsolver, HighsInt& len,
             const HighsInt*& inds, const double*& vals) const;

    HighsInt getRowLen(const HighsMipSolver& mipsolver) const;

    bool isIntegral(const HighsMipSolver& mipsolver) const;

    double getMaxAbsVal(const HighsMipSolver& mipsolver) const;

    static LpRow cut(HighsInt index) { return LpRow{kCutPool, index, 0}; }
    static LpRow model(HighsInt index) { return LpRow{kModel, index, 0}; }
  };

  const HighsMipSolver& mipsolver;
  Highs lpsolver;

  std::vector<LpRow> lprows;

  std::vector<std::pair<HighsInt, double>> fractionalints;
  std::vector<double> dualproofvals;
  std::vector<HighsInt> dualproofinds;
  std::vector<double> dualproofbuffer;
  std::vector<double> colLbBuffer;
  std::vector<double> colUbBuffer;
  double dualproofrhs;
  bool hasdualproof;
  double objective;
  std::shared_ptr<const HighsBasis> basischeckpoint;
  bool currentbasisstored;
  int64_t numlpiters;
  double avgSolveIters;
  int64_t numSolved;
  size_t epochs;
  HighsInt maxNumFractional;
  Status status;
  bool adjustSymBranchingCol;

  void storeDualInfProof();

  void storeDualUBProof();

  bool checkDualProof() const;

 public:
  HighsLpRelaxation(const HighsMipSolver& mip);

  HighsLpRelaxation(const HighsLpRelaxation& other);

  void loadModel();

  void getRow(HighsInt row, HighsInt& len, const HighsInt*& inds,
              const double*& vals) const {
    if (row < mipsolver.numRow())
      assert(lprows[row].origin == LpRow::Origin::kModel);
    else
      assert(lprows[row].origin == LpRow::Origin::kCutPool);
    lprows[row].get(mipsolver, len, inds, vals);
  }

  bool isRowIntegral(HighsInt row) const {
    assert(row < (HighsInt)lprows.size());
    return lprows[row].isIntegral(mipsolver);
  }

  void setAdjustSymmetricBranchingCol(bool adjustSymBranchingCol) {
    this->adjustSymBranchingCol = adjustSymBranchingCol;
  }

  double getAvgSolveIters() { return avgSolveIters; }

  HighsInt getRowLen(HighsInt row) const {
    return lprows[row].getRowLen(mipsolver);
  }

  double getMaxAbsRowVal(HighsInt row) const {
    return lprows[row].getMaxAbsVal(mipsolver);
  }

  const HighsLp& getLp() const { return lpsolver.getLp(); }

  const HighsSolution& getSolution() const { return lpsolver.getSolution(); }

  double slackUpper(HighsInt row) const;

  double slackLower(HighsInt row) const;

  double rowLower(HighsInt row) const {
    return lpsolver.getLp().row_lower_[row];
  }

  double rowUpper(HighsInt row) const {
    return lpsolver.getLp().row_upper_[row];
  }

  double colLower(HighsInt col) const {
    return col < lpsolver.getLp().num_col_
               ? lpsolver.getLp().col_lower_[col]
               : slackLower(col - lpsolver.getLp().num_col_);
  }

  double colUpper(HighsInt col) const {
    return col < lpsolver.getLp().num_col_
               ? lpsolver.getLp().col_upper_[col]
               : slackUpper(col - lpsolver.getLp().num_col_);
  }

  bool isColIntegral(HighsInt col) const {
    return col < lpsolver.getLp().num_col_
               ? mipsolver.variableType(col) != HighsVarType::kContinuous
               : isRowIntegral(col - lpsolver.getLp().num_col_);
  }

  double solutionValue(HighsInt col) const {
    return col < lpsolver.getLp().num_col_
               ? getSolution().col_value[col]
               : getSolution().row_value[col - lpsolver.getLp().num_col_];
  }

  Status getStatus() const { return status; }

  int64_t getNumLpIterations() const { return numlpiters; }

  bool integerFeasible() const {
    if ((status == Status::kOptimal ||
         status == Status::kUnscaledPrimalFeasible) &&
        fractionalints.empty())
      return true;

    return false;
  }

  double computeBestEstimate(const HighsPseudocost& ps) const;

  double computeLPDegneracy(const HighsDomain& localdomain) const;

  static bool scaledOptimal(Status status) {
    switch (status) {
      case Status::kOptimal:
      case Status::kUnscaledDualFeasible:
      case Status::kUnscaledPrimalFeasible:
      case Status::kUnscaledInfeasible:
        return true;
      default:
        return false;
    }
  }

  static bool unscaledPrimalFeasible(Status status) {
    switch (status) {
      case Status::kOptimal:
      case Status::kUnscaledPrimalFeasible:
        return true;
      default:
        return false;
    }
  }

  static bool unscaledDualFeasible(Status status) {
    switch (status) {
      case Status::kOptimal:
      case Status::kUnscaledDualFeasible:
        return true;
      default:
        return false;
    }
  }

  void recoverBasis();

  void setObjectiveLimit(double objlim = kHighsInf);

  void storeBasis() {
    if (!currentbasisstored && lpsolver.getBasis().valid) {
      basischeckpoint = std::make_shared<HighsBasis>(lpsolver.getBasis());
      currentbasisstored = true;
    }
  }

  std::shared_ptr<const HighsBasis> getStoredBasis() const {
    return basischeckpoint;
  }

  void setStoredBasis(std::shared_ptr<const HighsBasis> basis) {
    basischeckpoint = std::move(basis);
    currentbasisstored = false;
  }

  const HighsMipSolver& getMipSolver() const { return mipsolver; }

  HighsInt getNumModelRows() const { return mipsolver.numRow(); }

  HighsInt numRows() const { return lpsolver.getNumRow(); }

  HighsInt numCols() const { return lpsolver.getNumCol(); }

  HighsInt numNonzeros() const { return lpsolver.getNumNz(); }

  void addCuts(HighsCutSet& cutset);

  void performAging(bool useBasis = true);

  void resetAges();

  void removeObsoleteRows(bool notifyPool = true);

  void removeCuts(HighsInt ndelcuts, std::vector<HighsInt>& deletemask);

  void removeCuts();

  void flushDomain(HighsDomain& domain, bool continuous = false);

  void getDualProof(const HighsInt*& inds, const double*& vals, double& rhs,
                    HighsInt& len) {
    inds = dualproofinds.data();
    vals = dualproofvals.data();
    rhs = dualproofrhs;
    len = dualproofinds.size();
  }

  bool computeDualProof(const HighsDomain& globaldomain, double upperbound,
                        std::vector<HighsInt>& inds, std::vector<double>& vals,
                        double& rhs, bool extractCliques = true) const;

  bool computeDualInfProof(const HighsDomain& globaldomain,
                           std::vector<HighsInt>& inds,
                           std::vector<double>& vals, double& rhs);

  Status resolveLp(HighsDomain* domain = nullptr);

  Status run(bool resolve_on_error = true);

  Highs& getLpSolver() { return lpsolver; }
  const Highs& getLpSolver() const { return lpsolver; }

  const std::vector<std::pair<HighsInt, double>>& getFractionalIntegers()
      const {
    return fractionalints;
  }

  std::vector<std::pair<HighsInt, double>>& getFractionalIntegers() {
    return fractionalints;
  }

  double getObjective() const { return objective; }

  void setIterationLimit(HighsInt limit = kHighsIInf) {
    lpsolver.setOptionValue("simplex_iteration_limit", limit);
  }
};

#endif
