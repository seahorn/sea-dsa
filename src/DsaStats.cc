#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringRef.h"

#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/Info.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Stats.hh"

namespace sea_dsa {

  using namespace llvm;

  typedef DsaInfo::live_nodes_const_range nodes_range;
  typedef DsaInfo::alloc_sites_set alloc_sites_set;

  static void printMemAccesses(nodes_range nodes, llvm::raw_ostream &o) {
    // Here counters
    unsigned int total_accesses = 0; // count the number of total memory accesses

    o << " --- Memory access information\n";
    for (auto const &n: nodes) { total_accesses += n.getAccesses(); }

    o << "\t" << std::distance(nodes.begin(), nodes.end())
      << " number of read or modified nodes.\n"
      << "\t" << total_accesses
      << " number of non-trivial memory accesses"
      << " (load/store/memcpy/memset/memmove).\n";

    //  --- Print a summary of accesses
    std::vector<NodeWrapper> sorted_nodes(nodes.begin(), nodes.end());
    unsigned int summ_size = 5;
    o << "\t Summary of the " << summ_size << " nodes with more accesses:\n";
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
              [](const NodeWrapper &n1, const NodeWrapper &n2) {
                return (n1.getAccesses() > n2.getAccesses());
              });

    if (total_accesses > 0) {
      for (const auto &n: sorted_nodes) {
        if (summ_size <= 0) break;
        summ_size--;
        if (n.getAccesses() == 0) break;
        o << "\t  [Node Id " << n.getId() << "] "
          << n.getAccesses() << " ("
          << (int) (n.getAccesses() * 100 / total_accesses)
          << "%) of total memory accesses\n";
      }
    }
  }

  static void printMemTypes(nodes_range nodes, llvm::raw_ostream &o) {
    // Here counters
    unsigned num_offset_collapses = 0;    // number of offset-collapsed nodes
    unsigned num_typed_nodes = 0;         // number of typed nodes
    unsigned num_untyped_nodes = 0;
    unsigned num_fields = 0;
    unsigned num_nodes_multi_typed_fields = 0;

    o << " --- Node information\n";
    for (const auto &n: nodes) {
      if (n.getNode()->isOffsetCollapsed()) {
        num_offset_collapses++;
      } else {
        if (n.getNode()->types().empty())
          num_untyped_nodes++;
        else
          num_typed_nodes++;
      }
      num_fields += n.getNode()->getLinks().size();

      llvm::SmallDenseSet<unsigned, 8> fields;
      for (auto &link : n.getNode()->getLinks()) {
        unsigned offset = link.first.getOffset();
        if (fields.count(offset)) {
          ++num_nodes_multi_typed_fields;
          break;
        }
        fields.insert(offset);
      }
    }

    const unsigned total_nodes = std::distance(nodes.begin(), nodes.end());
    o << "\t" << total_nodes << " number of nodes\n";
    o << "\t\t" << num_typed_nodes << " typed nodes\n";
    o << "\t\t" << num_untyped_nodes << " untyped nodes\n";
    o << "\t\t" << num_offset_collapses << " offset-collapsed\n";
    o << "\t\t" << num_nodes_multi_typed_fields << " with multi-typed fields\n";
    o << "\t" << num_fields << " number of fields\n";

    // TODO: print all node's types
  }

  static void printAllocSites(nodes_range nodes,
                              const alloc_sites_set &alloc_sites,
                              llvm::raw_ostream &o) {
    /// number of nodes without allocation sites
    unsigned num_orphan_nodes = 0;
    /// number of checks belonging to orphan nodes.
    unsigned num_orphan_checks = 0;
    /// keep track of maximum number of allocation sites in a single node
    unsigned max_alloc_sites = 0;
    /// number of nodes with allocation sites with more than one type
    unsigned num_non_singleton = 0;
    /// total number of allocation sites
    unsigned num_alloc_sites = std::distance(alloc_sites.begin(),
                                             alloc_sites.end());

    o << " --- Allocation site information\n";

    // iterate over all nodes and update some counters
    for (const auto &n: nodes) {
      unsigned num_alloc_sites = n.getNode()->getAllocSites().size();
      if (num_alloc_sites == 0) {
        num_orphan_nodes++;
        num_orphan_checks += n.getAccesses();
      } else
        max_alloc_sites = std::max(max_alloc_sites, num_alloc_sites);
    }

    // count the number of nodes with more than one allocation site
    for (const auto &n: nodes) {
      SmallPtrSet<Type *, 32> allocTypes;
      for (const llvm::Value *v : n.getNode()->getAllocSites())
        allocTypes.insert(v->getType());
      num_non_singleton += (allocTypes.size() > 1);
    }

    // ufo::Stats::uset ("DsaNumOfAllocationSites", num_alloc_sites);

    o << "\t" << num_alloc_sites << " number of allocation sites\n";
    o << "\t   " << max_alloc_sites
      << " max number of allocation sites in a node\n";
    o << "\t" << num_orphan_nodes
      << " number of nodes without allocation site\n";
    o << "\t" << num_orphan_checks
      << " number of memory accesses without allocation site\n";
    o << "\t" << num_non_singleton
      << " number of nodes with multi-typed allocation sites\n";
  }
  

  void DsaPrintStats::runOnModule (Module &M)  {
    auto dsa_nodes = m_dsa.live_nodes();
    auto const &dsa_alloc_sites = m_dsa.alloc_sites();
    
    errs() << " ========== Begin SeaDsa stats  ==========\n";
    printMemAccesses(dsa_nodes, errs());
    printMemTypes(dsa_nodes, errs());
    printAllocSites(dsa_nodes, dsa_alloc_sites, errs());
    errs() << " ========== End SeaDsa stats  ==========\n";
  }
    
  Pass *createDsaPrintStatsPass() {
    return new DsaAnalysis(true);
  }
  
} // end namespace sea_dsa

