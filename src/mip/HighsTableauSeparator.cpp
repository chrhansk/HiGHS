/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2020 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file mip/HighsTableauSeparator.cpp
 * @author Leona Gottwald
 */

#include "mip/HighsTableauSeparator.h"

#include "mip/HighsCutGeneration.h"
#include "mip/HighsLpAggregator.h"
#include "mip/HighsLpRelaxation.h"
#include "mip/HighsMipSolverData.h"
#include "mip/HighsTransformedLp.h"

void HighsTableauSeparator::separateLpSolution(HighsLpRelaxation& lpRelaxation,
                                               HighsLpAggregator& lpAggregator,
                                               HighsTransformedLp& transLp,
                                               HighsCutPool& cutpool) {
  std::vector<int> basisinds;
  Highs& lpSolver = lpRelaxation.getLpSolver();
  int numrow = lpRelaxation.numRows();
  basisinds.resize(numrow);
  lpRelaxation.getLpSolver().getBasicVariables(basisinds.data());

  std::vector<int> nonzeroWeights;
  std::vector<double> rowWeights;
  nonzeroWeights.resize(numrow);
  rowWeights.resize(numrow);
  int numNonzeroWeights;

  HighsCutGeneration cutGen(lpRelaxation, cutpool);

  std::vector<int> baseRowInds;
  std::vector<double> baseRowVals;

  const HighsMipSolver& mip = lpRelaxation.getMipSolver();

  const HighsSolution& lpSolution = lpRelaxation.getSolution();
  size_t maxbaserowlen = 100 + 0.1 * (mip.numCol() + mip.numRow());

  for (int i = 0; i != int(basisinds.size()); ++i) {
    double fractionality;
    if (basisinds[i] < 0) {
      int row = -basisinds[i] - 1;

      if (!lpRelaxation.isRowIntegral(row)) continue;

      double solval = lpSolution.row_value[row];
      fractionality = std::abs(std::round(solval) - solval);
    } else {
      int col = basisinds[i];
      if (mip.variableType(col) == HighsVarType::CONTINUOUS) continue;

      double solval = lpSolution.col_value[col];
      fractionality = std::abs(std::round(solval) - solval);
    }

    if (fractionality < 1e-4) continue;
    if (lpSolver.getBasisInverseRow(i, rowWeights.data(), &numNonzeroWeights,
                                    nonzeroWeights.data()) != HighsStatus::OK)
      continue;

    for (int j = 0; j != numNonzeroWeights; ++j) {
      int row = nonzeroWeights[j];
      if (std::abs(rowWeights[row]) <= mip.mipdata_->epsilon) continue;
      lpAggregator.addRow(row, rowWeights[row]);
    }

    lpAggregator.getCurrentAggregation(baseRowInds, baseRowVals, false);

    //if (baseRowInds.size() > maxbaserowlen) continue;

    double rhs = 0;
    cutGen.generateCut(transLp, baseRowInds, baseRowVals, rhs);

    lpAggregator.getCurrentAggregation(baseRowInds, baseRowVals, true);
    rhs = 0;
    cutGen.generateCut(transLp, baseRowInds, baseRowVals, rhs);

    lpAggregator.clear();
  }
}
