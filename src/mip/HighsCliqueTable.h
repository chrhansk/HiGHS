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
#ifndef HIGHS_CLIQUE_TABLE_H_
#define HIGHS_CLIQUE_TABLE_H_

#include <cstdint>
#include <set>
#include <vector>

#include "lp_data/HConst.h"
#include "util/HighsHash.h"
#include "util/HighsRandom.h"

class HighsCutPool;
class HighsDomain;
class HighsMipSolver;
class HighsLp;

class HighsCliqueTable {
 public:
  struct CliqueVar {
#ifdef HIGHSINT64
    HighsUInt col : 63;
    HighsUInt val : 1;
#else
    HighsUInt col : 31;
    HighsUInt val : 1;
#endif

    HighsInt index() const { return 2 * col + val; }

    double weight(const std::vector<double>& sol) const {
      return val ? sol[col] : 1.0 - sol[col];
    }

    CliqueVar complement() const { return CliqueVar(col, 1 - val); }

    bool operator==(const CliqueVar& other) const {
      return index() == other.index();
    }

    CliqueVar(HighsInt col, HighsInt val) : col(col), val(val) {}
    CliqueVar() = default;
  };
  struct Clique {
    HighsInt start;
    HighsInt end;
    HighsInt origin;
    HighsInt numZeroFixed;
    bool equality;
  };

  struct Substitution {
    HighsInt substcol;
    CliqueVar replace;
  };

 private:
  struct CliqueSetNode {
    HighsInt cliqueid;
    // links for storing the column lists of the clique as a splay tree
    HighsInt left;
    HighsInt right;

    CliqueSetNode(HighsInt cliqueid)
        : cliqueid(cliqueid), left(-1), right(-1) {}

    CliqueSetNode() : cliqueid(-1), left(-1), right(-1) {}
  };

  std::vector<CliqueVar> cliqueentries;
  std::vector<CliqueSetNode> cliquesets;

  std::vector<std::pair<HighsInt*, HighsInt*>> commoncliquestack;
  std::set<std::pair<HighsInt, int>> freespaces;
  std::vector<HighsInt> freeslots;
  std::vector<Clique> cliques;
  std::vector<HighsInt> cliquesetroot;
  std::vector<HighsInt> sizeTwoCliquesetRoot;
  std::vector<HighsInt> numcliquesvar;
  std::vector<CliqueVar> infeasvertexstack;

  std::vector<HighsInt> colsubstituted;
  std::vector<Substitution> substitutions;
  std::vector<HighsInt> deletedrows;
  std::vector<std::pair<HighsInt, CliqueVar>> cliqueextensions;
  std::vector<uint8_t> iscandidate;
  std::vector<uint8_t> colDeleted;
  std::vector<uint32_t> cliquehits;
  std::vector<HighsInt> cliquehitinds;
  std::vector<HighsInt> stack;
  std::vector<uint8_t> neighborhoodFlags;

  // HighsHashTable<std::pair<CliqueVar, CliqueVar>> invertedEdgeCache;
  HighsHashTable<std::pair<CliqueVar, CliqueVar>, HighsInt> sizeTwoCliques;

  HighsRandom randgen;
  HighsInt nfixings;
  HighsInt numEntries;
  HighsInt maxEntries;
  bool inPresolve;
  HighsInt splay(int64_t& numQueries, HighsInt cliqueid, HighsInt root);

  void unlink(HighsInt node);

  void link(HighsInt node);

  HighsInt findCommonCliqueId(int64_t& numQueries, CliqueVar v1, CliqueVar v2);

  HighsInt findCommonCliqueId(CliqueVar v1, CliqueVar v2) {
    return findCommonCliqueId(numNeighborhoodQueries, v1, v2);
  }

  HighsInt runCliqueSubsumption(const HighsDomain& globaldom,
                                std::vector<CliqueVar>& clique);
  struct BronKerboschData {
    const std::vector<double>& sol;
    std::vector<CliqueVar> P;
    std::vector<CliqueVar> R;
    std::vector<CliqueVar> Z;
    std::vector<std::vector<CliqueVar>> cliques;
    double wR = 0.0;
    double minW = 1.05;
    double feastol = 1e-6;
    HighsInt ncalls = 0;
    HighsInt maxcalls = 10000;
    HighsInt maxcliques = 100;
    int64_t maxNeighborhoodQueries = std::numeric_limits<int64_t>::max();

    bool stop(int64_t numNeighborhoodQueries) const {
      return maxcalls == ncalls || int(cliques.size()) == maxcliques ||
             numNeighborhoodQueries > maxNeighborhoodQueries;
    }

    BronKerboschData(const std::vector<double>& sol) : sol(sol) {}
  };

  void bronKerboschRecurse(BronKerboschData& data, HighsInt Plen,
                           const CliqueVar* X, HighsInt Xlen);

  void extractCliques(const HighsMipSolver& mipsolver,
                      std::vector<HighsInt>& inds, std::vector<double>& vals,
                      std::vector<int8_t>& complementation, double rhs,
                      HighsInt nbin, std::vector<HighsInt>& perm,
                      std::vector<CliqueVar>& clique, double feastol);

  void processInfeasibleVertices(HighsDomain& domain);

  void propagateAndCleanup(HighsDomain& globaldom);

  void doAddClique(const CliqueVar* cliquevars, HighsInt numcliquevars,
                   bool equality = false, HighsInt origin = kHighsIInf);

  void queryNeighborhood(CliqueVar v, CliqueVar* q, HighsInt N);

 public:
  int64_t numNeighborhoodQueries;

  HighsCliqueTable(HighsInt ncols) {
    cliquesetroot.resize(2 * ncols, -1);
    sizeTwoCliquesetRoot.resize(2 * ncols, -1);
    numcliquesvar.resize(2 * ncols, 0);
    neighborhoodFlags.resize(2 * ncols, 0);
    colsubstituted.resize(ncols);
    colDeleted.resize(ncols, false);
    nfixings = 0;
    numNeighborhoodQueries = 0;
    numEntries = 0;
    maxEntries = kHighsIInf;
    inPresolve = false;
  }

  void setPresolveFlag(bool inPresolve) { this->inPresolve = inPresolve; }

  HighsInt getNumEntries() const { return numEntries; }

  HighsInt partitionNeighborhood(CliqueVar v, CliqueVar* q, HighsInt N);

  HighsInt shrinkToNeighborhood(CliqueVar v, CliqueVar* q, HighsInt N);

  bool processNewEdge(HighsDomain& globaldom, CliqueVar v1, CliqueVar v2);

  void addClique(const HighsMipSolver& mipsolver, CliqueVar* cliquevars,
                 HighsInt numcliquevars, bool equality = false,
                 HighsInt origin = kHighsIInf);

  void removeClique(HighsInt cliqueid);

  void resolveSubstitution(CliqueVar& v) const;

  void resolveSubstitution(HighsInt& col, double& val, double& rhs) const;

  std::vector<HighsInt>& getDeletedRows() { return deletedrows; }

  const std::vector<HighsInt>& getDeletedRows() const { return deletedrows; }

  std::vector<Substitution>& getSubstitutions() { return substitutions; }

  const std::vector<Substitution>& getSubstitutions() const {
    return substitutions;
  }

  const Substitution* getSubstitution(HighsInt col) const {
    return colsubstituted[col] ? &substitutions[colsubstituted[col] - 1]
                               : nullptr;
  }

  std::vector<std::pair<HighsInt, CliqueVar>>& getCliqueExtensions() {
    return cliqueextensions;
  }

  const std::vector<std::pair<HighsInt, CliqueVar>>& getCliqueExtensions()
      const {
    return cliqueextensions;
  }

  void setMaxEntries(HighsInt numNz) {
    this->maxEntries = 2000000 + 10 * numNz;
  }

  bool isFull() const { return numEntries >= maxEntries; }

  HighsInt getNumFixings() const { return nfixings; }

  bool foundCover(HighsDomain& globaldom, CliqueVar v1, CliqueVar v2);

  void extractCliques(HighsMipSolver& mipsolver, bool transformRows = true);

  void extractCliquesFromCut(const HighsMipSolver& mipsolver,
                             const HighsInt* inds, const double* vals,
                             HighsInt len, double rhs);

  void extractObjCliques(HighsMipSolver& mipsolver);

  void vertexInfeasible(HighsDomain& globaldom, HighsInt col, HighsInt val);

  bool haveCommonClique(CliqueVar v1, CliqueVar v2) {
    if (v1.col == v2.col) return false;
    return findCommonCliqueId(v1, v2) != -1;
  }

  bool haveCommonClique(int64_t& numQueries, CliqueVar v1, CliqueVar v2) {
    if (v1.col == v2.col) return false;
    return findCommonCliqueId(numQueries, v1, v2) != -1;
  }

  std::pair<const CliqueVar*, HighsInt> findCommonClique(CliqueVar v1,
                                                         CliqueVar v2) {
    std::pair<const CliqueVar*, HighsInt> c{nullptr, 0};
    if (v1 == v2) return c;
    HighsInt clq = findCommonCliqueId(v1, v2);
    if (clq == -1) return c;

    c.first = &cliqueentries[cliques[clq].start];
    c.second = cliques[clq].end - cliques[clq].start;
    return c;
  }

  void separateCliques(const HighsMipSolver& mipsolver,
                       const std::vector<double>& sol, HighsCutPool& cutpool,
                       double feastol);

  std::vector<std::vector<CliqueVar>> separateCliques(
      const std::vector<double>& sol, const HighsDomain& globaldom,
      double feastol);

  void cleanupFixed(HighsDomain& globaldom);

  void addImplications(HighsDomain& domain, HighsInt col, HighsInt val);

  HighsInt getNumImplications(HighsInt col);

  HighsInt getNumImplications(HighsInt col, bool val);

  void runCliqueMerging(HighsDomain& globaldomain);

  void runCliqueMerging(HighsDomain& globaldomain,
                        std::vector<CliqueVar>& clique, bool equation = false);

  void rebuild(HighsInt ncols, const HighsDomain& globaldomain,
               const std::vector<HighsInt>& cIndex,
               const std::vector<HighsInt>& rIndex);

  void buildFrom(const HighsLp* origModel, const HighsCliqueTable& init);

  HighsInt numCliques() const { return cliques.size() - freeslots.size(); }

  HighsInt numCliques(HighsInt col, bool val) const {
    return numcliquesvar[CliqueVar(col, val).index()];
  }
};

#endif
