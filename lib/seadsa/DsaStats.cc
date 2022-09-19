#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include "seadsa/DsaAnalysis.hh"
#include "seadsa/Graph.hh"
#include "seadsa/Info.hh"
#include "seadsa/Stats.hh"

static llvm::cl::opt<unsigned> NodeSummarySize(
    "sea-dsa-stats-node-summary-size",
    llvm::cl::desc("Maximum number of nodes shown in the node summary"),
    llvm::cl::Hidden, llvm::cl::init(20));

static llvm::cl::opt<unsigned> TypeDisparitySummarySize(
    "sea-dsa-stats-type-disparity-summary-size",
    llvm::cl::desc("Maximum number of allocation sites shown in the "
                   "type-disparity summary"),
    llvm::cl::Hidden, llvm::cl::init(20));

namespace seadsa {

using namespace llvm;

using nodes_range = DsaInfo::live_nodes_const_range;
using alloc_sites_set = DsaInfo::alloc_sites_set;

static void printMemUsageInfo(nodes_range nodes, llvm::raw_ostream &o) {
  // Here counters
  unsigned int total_accesses = 0; // count the number of total memory accesses

  o << "--- Memory access information\n";
  for (auto const &n : nodes) {
    total_accesses += n.getAccesses();
  }

  // o << "\t" << std::distance(nodes.begin(), nodes.end())
  //   << " number of read or modified points-to graph nodes.\n"
  o << "\t" << total_accesses << " number of non-trivial memory accesses"
    << " (load/store/memcpy/memset/memmove).\n";

  //  --- Print a summary of accesses
  std::vector<NodeInfo> sorted_nodes(nodes.begin(), nodes.end());
  unsigned int summ_size = NodeSummarySize;
  o << "\tSummary of the " << summ_size << " nodes with more accesses:\n";
  std::sort(sorted_nodes.begin(), sorted_nodes.end(),
            [](const NodeInfo &n1, const NodeInfo &n2) {
              return (n1.getAccesses() > n2.getAccesses());
            });

  if (total_accesses > 0) {
    for (const auto &n : sorted_nodes) {
      if (summ_size <= 0) break;
      summ_size--;
      if (n.getAccesses() == 0) break;
      o << "\t  [Node Id " << n.getId() << "] " << n.getAccesses() << " ("
        << (int)(n.getAccesses() * 100 / total_accesses)
        << "%) of total memory accesses "
        << "IsOffsetCollapsed=" << n.getNode()->isOffsetCollapsed() << " "
        << "IsTypeCollapsed=" << n.getNode()->isTypeCollapsed() << " "
        << "IsSequence=" << n.getNode()->isArray() << "\n";
    }
  }
}

static void printGraphInfo(nodes_range nodes, llvm::raw_ostream &o) {
  unsigned total_nodes = 0;
  /// Stats about nodes in the graph
  unsigned num_offset_collapses = 0; // number of offset-collapsed nodes
  unsigned num_typed_nodes = 0;      // number of typed nodes
  unsigned num_untyped_nodes = 0;
  unsigned num_nodes_multi_typed_fields = 0;
  /// Stats about edges in the graph
  unsigned num_links = 0;
  unsigned num_max_links = 0;

  for (const auto &n : nodes) {
    total_nodes++;
    if (n.getNode()->isOffsetCollapsed()) {
      num_offset_collapses++;
    } else {
      if (n.getNode()->types().empty())
        num_untyped_nodes++;
      else
        num_typed_nodes++;
    }
    num_max_links = std::max(num_max_links, n.getNode()->getNumLinks());
    num_links += n.getNode()->getNumLinks();
  }

  o << "--- Points-to graph information\n";
  o << "\t" << total_nodes << " number of modified/read nodes\n";
  o << "\t\t" << num_typed_nodes << " typed nodes\n";
  o << "\t\t" << num_untyped_nodes << " untyped nodes\n";
  o << "\t\t" << num_offset_collapses << " offset-collapsed\n";
  o << "\t\t" << num_nodes_multi_typed_fields << " with multi-typed fields\n";
  if (total_nodes > 0) {
    o << "\t" << (double)num_links / (double)total_nodes
      << " average number of node links (graph fan-out)\n";
    o << "\t" << num_max_links << " maximum number of links of a given node\n";
  }
}

static void printAllocInfo(nodes_range nodes,
                           const alloc_sites_set &alloc_sites,
                           llvm::raw_ostream &o) {
  /// number of nodes without allocation sites
  unsigned num_orphan_nodes = 0;
  /// number of checks belonging to orphan nodes.
  unsigned num_orphan_checks = 0;
  /// keep track of maximum number of allocation sites in a single node
  unsigned max_alloc_sites = 0;
  /// total number of allocation sites
  unsigned total_num_alloc_sites =
      std::distance(alloc_sites.begin(), alloc_sites.end());
  // to print type disparity in allocation sizes
  std::vector<std::pair<const NodeInfo *, SmallPtrSet<Type *, 32>>>
      typeDisparity;

  // iterate over all nodes and update some counters
  for (const auto &n : nodes) {
    unsigned num_alloc_sites = n.getNode()->getAllocSites().size();
    if (num_alloc_sites == 0) {
      num_orphan_nodes++;
      num_orphan_checks += n.getAccesses();
    } else {
      max_alloc_sites = std::max(max_alloc_sites, num_alloc_sites);
    }

    // convert from a set of Value* to a set of Type*
    SmallPtrSet<Type *, 32> allocTypes;
    for (const llvm::Value *v : n.getNode()->getAllocSites()) {
      allocTypes.insert(v->getType());
    }
    typeDisparity.push_back(
        std::pair<const NodeInfo *, SmallPtrSet<Type *, 32>>(&n, allocTypes));
  }

  // Sort by size of the types in descending order
  std::sort(typeDisparity.begin(), typeDisparity.end(),
            [](const std::pair<const NodeInfo *, SmallPtrSet<Type *, 32>> &p1,
               const std::pair<const NodeInfo *, SmallPtrSet<Type *, 32>> &p2) {
              unsigned sz_p1 = p1.second.size();
              unsigned sz_p2 = p2.second.size();
              if (sz_p1 > sz_p2) {
                return true;
              } else if (sz_p1 < sz_p2) {
                return false;
              } else {
                return p1.first->getId() < p2.first->getId();
              }
            });

  o << "--- Allocation site information\n";
  o << "\t" << total_num_alloc_sites << " number of allocation sites\n";
  o << "\t   " << max_alloc_sites
    << " max number of allocation sites in a node\n";
  o << "\t" << num_orphan_nodes << " number of nodes without allocation site\n";
  o << "\t" << num_orphan_checks
    << " number of memory accesses without allocation site\n";
  o << "\tType disparity in allocation sites:\n";
  unsigned sz =  (TypeDisparitySummarySize < typeDisparity.size() ?
		  TypeDisparitySummarySize : typeDisparity.size());
  for (unsigned i = 0; i < sz; ++i) {
    const NodeInfo *nodei = typeDisparity[i].first;
    o << "\t\t [Node Id " << nodei->getId()
      << "] IsCollapsed=" << nodei->getNode()->isOffsetCollapsed() << " has "
      << typeDisparity[i].second.size() << " different types: {";
    for (Type *ty : typeDisparity[i].second) {
      o << *ty << ";";
    }
    o << "}\n";
  }
}

void DsaPrintStats::runOnModule(Module &M) {
  auto dsa_nodes = m_dsa.live_nodes();
  auto const &dsa_alloc_sites = m_dsa.alloc_sites();

  errs() << " ========== Begin SeaDsa stats  ==========\n";
  printMemUsageInfo(dsa_nodes, errs());
  printGraphInfo(dsa_nodes, errs());
  printAllocInfo(dsa_nodes, dsa_alloc_sites, errs());
  errs() << " ========== End SeaDsa stats  ==========\n";
}

Pass *createDsaPrintStatsPass() { return new DsaAnalysis(true); }

} // end namespace seadsa
