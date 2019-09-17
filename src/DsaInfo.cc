#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Analysis/MemoryBuiltins.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"

#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Info.hh"
#include "sea_dsa/config.h"
#include "sea_dsa/support/Debug.h"

static llvm::cl::opt<std::string> DsaToCsv(
    "sea-dsa-to-csv",
    llvm::cl::desc(
        "SeaDsa: print pairs of allocation site and Dsa node into a CSV file"),
    llvm::cl::init(""), llvm::cl::Hidden);

static llvm::cl::opt<std::string>
AllocasToFile("sea-dsa-allocas-to-file",
	      llvm::cl::desc ("SeaDsa: print allocation sites into a file"),
	      llvm::cl::init(""), llvm::cl::Hidden);

using namespace sea_dsa;
using namespace llvm;

static bool isStaticallyKnown (const DataLayout* dl, 
                              const TargetLibraryInfo *tli, const Value *v) {
  uint64_t size;
  ObjectSizeOpts opts;
  opts.RoundToAlign = true;
  if (getObjectSize(v, size, *dl, tli, opts))
    return (size > 0);
  return false; 
}

static bool isIntToPtrConstant(const llvm::Value &v) {
  if (auto *inttoptr = dyn_cast<ConstantExpr>(&v)) {
    if (inttoptr->getOpcode() == Instruction::IntToPtr) {
      return true;
    }
  }
  return false;
}


static StringRef valueToStr(const Value*v) {
  if (const Constant *c = dyn_cast<const Constant>(v)) {
    if (const ConstantInt *n = dyn_cast<const ConstantInt>(c)) {
      const APInt &ai = n->getValue();
      SmallString<128> str;      
      ai.toStringSigned(str, 10);
      return str.str();
    }
  }
  
  if (!v->hasName() && !isIntToPtrConstant(*v)) {
    static bool warned = false;
    if (!warned) {
      errs() << "Warning: DsaInfo requires " << *v << " to have a name\n";
      warned = true;
    }

    // Use addresses when instruction has no name but we want to sort it anyway.
    // IDs are stable as long as the invocations of this functions are stable,
    // so this can potentially result in non-determinism in results.
    static llvm::DenseMap<const Value *, std::string> adHocNameCache;
    static size_t adHocId = 0;
    auto it = adHocNameCache.find(v);
    if (it == adHocNameCache.end()) {
      std::string name;
      raw_string_ostream rso(name);
      if (auto *i = dyn_cast<const Instruction>(v))
        rso << i->getFunction()->getName() << "_";

      rso << (adHocId++);
      rso.flush();

      adHocNameCache[v] = std::move(name);
      return adHocNameCache[v];
    }

    return it->second;
  }

  return v->getName();
}

static bool compareValues (const Value* v1, const Value* v2) {
  StringRef str1 = valueToStr(v1);
  StringRef str2 = valueToStr(v2);
  
  return (str1 < str2);
}

// return null if there is no graph for f
Graph* DsaInfo::getDsaGraph(const Function&f) const {
  Graph *g = nullptr;
  if (m_dsa.hasGraph(f))
    g = &m_dsa.getGraph(f);
  return g;
}

bool DsaInfo::is_alive_node::operator()(const NodeWrapper& n) {
  return n.getNode()->isRead() || n.getNode()->isModified();
}

void DsaInfo::recordMemAccess (const Value* v, Graph& g, const Instruction &I) {
  v = v->stripPointerCasts();

  if (isStaticallyKnown(&m_dl, &m_tli, v))
    return;
  
  if (!g.hasCell(*v)) {
    // sanity check
    if (v->getType()->isPointerTy())
      errs () << "WARNING DsaInfo: pointer value " << *v << " has not cell\n";
    return;
  }

  if (isa<GlobalValue>(v)) {
    // DSA pretends all global variables have a node by creating a new
    // fresh node each time getCell is called.
    return;
  }
  
  const Cell &c = g.getCell (*v);
  Node *n = c.getNode();
  auto it = m_nodes_map.find (n);
  if (it != m_nodes_map.end ()) {
    ++(it->second);
    #if 0
    if (c.getNode()->getAllocSites ().size () == 0) {
      errs () << "WARNING: " << I.getParent ()->getParent ()->getName () << ":"
	      << I << " has no allocation site\n";
    }
    #endif 
  }
}        

void DsaInfo::recordMemAccesses (const Function&F) {
  // A node can be read or modified even if there is no a memory
  // load/store for it. This can happen after nodes are unified.
  // 
  // Here we just count the number of **non-trivial** memory accesses
  // because it is useful for passes that will instrument them.
  auto g = getDsaGraph(F);
  if (!g)
    return;

  for (const_inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
    const Instruction *I = &*i;
    if (const LoadInst *LI = dyn_cast<LoadInst>(I)) {
      recordMemAccess (LI->getPointerOperand (), *g, *I);
    } else if (const StoreInst *SI = dyn_cast<StoreInst>(I)) {
      recordMemAccess (SI->getPointerOperand (), *g, *I);
    } else if (const MemTransferInst *MTI = dyn_cast<MemTransferInst>(I)) {
      recordMemAccess (MTI->getDest (), *g, *I);
      recordMemAccess (MTI->getSource (), *g, *I);
    } else if (const MemSetInst *MSI = dyn_cast<MemSetInst>(I)) {
      recordMemAccess (MSI->getDest (), *g, *I);
    }
  }
}
      
bool DsaInfo::recordAllocSite (const Value* v, unsigned &site_id) {
  auto it = m_alloc_sites_bimap.left.find (v);
  if (it == m_alloc_sites_bimap.left.end ()) {
    site_id = m_alloc_sites_bimap.size () + 1;
    m_alloc_sites_bimap.insert (typename AllocSiteBiMap::value_type (v, site_id));
    m_alloc_sites_set.insert (site_id);
    return true;
  } else {
    site_id = it->second;
    return false;
  }
}

void DsaInfo::assignAllocSiteId () {
  auto nodes = live_nodes ();
  
  /// XXX: sort nodes by id's to achieve determinism across different
  /// executions.
  std::vector<NodeWrapper> nodes_sorted;
  for (auto n : nodes) {
    nodes_sorted.push_back(n);
  }
  std::sort (nodes_sorted.begin(), nodes_sorted.end());

  // map each allocation site to a set of nodes
  DenseMap<const llvm::Value *, std::pair<unsigned, NodeWrapperSet>>
      alloc_to_nodes_map;
  
  // iterate over all sorted nodes
  for (const auto &n: nodes_sorted) {
    /// XXX: sort alloca sites by name to achieve determinism across
    /// different executions.
    
    // iterate over all allocation sites
    auto &n_alloca_sites = n.getNode ()->getAllocSites ();
    std::vector<const llvm::Value*> n_alloca_sites_sorted (n_alloca_sites.begin(),
							   n_alloca_sites.end());
    std::sort (n_alloca_sites_sorted.begin(),
	       n_alloca_sites_sorted.end(), compareValues);

    for (const llvm::Value*v : n_alloca_sites_sorted) {
      // assign a unique id to the allocation site for Dsa clients
      unsigned site_id;
      recordAllocSite (v, site_id);

      auto it = alloc_to_nodes_map.find (v);
      if (it != alloc_to_nodes_map.end())
        it->second.second.insert(n);
      else {
        NodeWrapperSet s;
        s.insert(n);
        alloc_to_nodes_map.insert(
            std::make_pair(v, std::make_pair(site_id, s)));
      }
    }
  }
  
  // --- write to a CSV file all pairs (alloc site, node id)
  if (DsaToCsv != "") {
    std::string filename (DsaToCsv);
    std::error_code EC;
    raw_fd_ostream file (filename, EC, sys::fs::F_Text);
    file << "alloc_site,ds_node\n";
    for (auto &kv: alloc_to_nodes_map)
      for (const auto &nodeInfo: kv.second.second)
        file <<  kv.second.first << "," << nodeInfo.getId () << "\n";
    file.close();
  }

  // --- write all allocation sites to a file
  if (AllocasToFile != "") {
    std::string filename (AllocasToFile);
    std::error_code EC;
    raw_fd_ostream file (filename, EC, sys::fs::F_Text);
    for (auto &kv: alloc_to_nodes_map) {
      file << "Alloc site Id " << kv.second.first << "\n";
      file << *(kv.first) << "\n";
      file << "Dsa Node Ids {";
      bool first = true;
      for (typename NodeWrapperSet::iterator it = kv.second.second.begin(),
                                             et = kv.second.second.end();
           it != et;) {
        if (!first)
          file << ",";
        else
          first = false;
	file << it->getId ();
	++it;
      }
      file << "}  " << *(kv.first) << "\n";
      file << "=========================================\n";
    }
  }

  // // --- print for each node the types of its allocation sites
  // LOG("sea-dsa-info-alloc-types",
  //     for (const auto &n: nodes)  {
  // 	SmallPtrSet<Type*, 32> allocTypes;
  // 	for (const llvm::Value*v : n.getNode ()->getAllocSites ())
  // 	  allocTypes.insert(v->getType());
  // 	if (allocTypes.size () > 1) {
  // 	  n.getNode()->write(o);
  // 	  errs() << "\nTypes of the node's allocation sites\n";
  // 	  for (auto ty : allocTypes) 
  // 	    errs() << "\t" << *ty << "\n";
  // 	}
  //     });
}
 
template <typename PairRange, typename SetMap>
inline void InsertReferrer (PairRange r, SetMap &m) {
  for (auto &kv : r) {
    const Node *n = kv.second->getNode ();
    if (!(n->isRead() || n->isModified()))
      continue;
    
    auto it = m.find (n);
    if (it != m.end ())
      it->second.insert (kv.first);
    else {
      typename SetMap::mapped_type s;
      s.insert (kv.first);
      m.insert(std::make_pair (n, s));
    }
  }
}

// Assign a unique name to a value for the whole module
std::string DsaInfo::getName (const Function&fn, const Value& v) {
  assert (v.hasName ());
  const Value * V = v.stripPointerCasts();
  
  auto it = m_names.find (V);
  if (it != m_names.end())
    return it->second;

  std::string name = fn.getName ().str() + "." + v.getName ().str();
  auto res = m_names.insert (std::make_pair (V, name));
  return res.first->second;

  // if (V->hasName ()) return V->getName().str();
  // auto it = m_names.find (V);
  // if (it != m_names.end ()) return it->second;
  // // FIXME: this can be really slow!
  // // XXX: I don't know a better way to obtain a string-like
  // // representation from a value.
  // std::string str("");
  // raw_string_ostream str_os (str);
  // V->print (str_os); 
  // auto res = m_names.insert (std::make_pair (V, str_os.str ()));
  // return res.first->second;
}

// Assign to each node a **deterministic** id that is preserved across
// different executions
void DsaInfo::assignNodeId (const Function& fn, Graph* g) {
  #if 1
  /// XXX: Assume class Node already assigns a global id to each node

  for (auto &kv :
       boost::make_iterator_range(g->scalar_begin(), g->scalar_end())) {
    const Node *n = kv.second->getNode ();
    m_nodes_map.insert(std::make_pair(n,NodeWrapper(n, n->getId(), "")));
  }                                   
                                      
  for (auto &kv :
       boost::make_iterator_range(g->formal_begin(), g->formal_end())) {
    const Node *n = kv.second->getNode ();
    m_nodes_map.insert(std::make_pair(n,NodeWrapper(n, n->getId(), "")));
  }                                   

  for (auto &kv :
       boost::make_iterator_range(g->return_begin(), g->return_end())) {
    const Node *n = kv.second->getNode ();
    m_nodes_map.insert(std::make_pair(n,NodeWrapper(n, n->getId(), "")));
  }                                   
                                               
  #else
  /// XXX: Try to build a global id from the name of a node's
  /// representative. The representative is chosen deterministically.

  typedef boost::unordered_map<const Node*, ValueSet  > ReferrerMap;
  typedef std::vector<std::pair<const Node*, std::string> > ReferrerRepVector;

  ReferrerMap ref_map;
  ReferrerRepVector sorted_ref_vector;

  // Build referrers for each node
  InsertReferrer(boost::make_iterator_range(g->scalar_begin(), g->scalar_end()),
                 ref_map);
  InsertReferrer(boost::make_iterator_range(g->formal_begin(), g->formal_end()),
                 ref_map);
  InsertReferrer(boost::make_iterator_range(g->return_begin(), g->return_end()),
                 ref_map);

  // Find *deterministically* a representative for each node from its
  // set of referrers
  for (auto &kv : ref_map) {
    const Node * n = kv.first;
    const ValueSet& refs = kv.second;

    // Transform addresses to strings 
    // XXX: StringRef does not own the string data so we cannot use here
    std::vector<std::string> named_refs;
    named_refs.reserve (refs.size ());
    for (auto v: refs) 
      if (v->hasName()) {
        /// XXX: constant expressions won't have names
        named_refs.push_back (getName (fn, *v));
      }
      
    // Sort strings
    std::sort (named_refs.begin (), named_refs.end ());

    // Choose the first one
    if (!named_refs.empty ()) 
      sorted_ref_vector.push_back(std::make_pair(n, named_refs [0]));
    else
      errs () << "WARNING DsaInfo: not found representative for node\n";
  }

  if (sorted_ref_vector.empty())
    return;

  // Sort nodes 
  unsigned num_ref = sorted_ref_vector.size ();
  std::sort (sorted_ref_vector.begin (), sorted_ref_vector.end (),
             [](std::pair<const Node*, std::string> p1, 
                std::pair<const Node*, std::string> p2){
               return (p1.second < p2.second);
             });

  if (sorted_ref_vector.size () != num_ref)
    errs () << "WARNING DsaInfo: node representatives should be unique\n";
  
  // -- Finally, assign a unique (deterministic) id to each node
  //    The id 0 is reserved in case some query goes wrong.
  for (auto &kv: sorted_ref_vector)
    m_nodes_map.insert(std::make_pair(
        kv.first, NodeWrapper(kv.first, m_nodes_map.size() + 1, kv.second)));
  #endif 
}

bool DsaInfo::runOnFunction (Function &f) {
  if (f.isDeclaration())
    return false;
  
  if (Graph* g = getDsaGraph(f)) {
    LOG("dsa-info", errs() << f.getName() << " has "
                           << std::distance(g->begin(), g->end())
                           << " nodes\n");

    // XXX: If the analysis is context-insensitive all functions have
    // the same graph so we just need to compute the id's for the
    // first graph.
    if (m_seen_graphs.count (g) <= 0)
      assignNodeId (f, g); 

    recordMemAccesses (f);
  } else {
    errs () << "WARNING: " << f.getName () << " has not Dsa graph\n";
  }
  return false;
}

bool DsaInfo::runOnModule (Module &M) {

  for (auto &f: M) {
    runOnFunction (f); 
  }
  assignAllocSiteId();
  return false;
}

///////////////////////////////////////////////////////////
// External API for Dsa clients
///////////////////////////////////////////////////////////

bool DsaInfo::isAccessed (const Node&n) const {
  auto it = m_nodes_map.find (&n);
  if (it != m_nodes_map.end ())
    return (it->second.getAccesses () > 0);
  else
    return false; // not found
}

unsigned int DsaInfo::getDsaNodeId (const Node&n) const {
  auto it = m_nodes_map.find (&n);
  if (it != m_nodes_map.end ())
    return it->second.getId ();
  else
    return 0; // not found
}

unsigned int DsaInfo::getAllocSiteId (const Value* V) const {
  auto it = m_alloc_sites_bimap.left.find (V);
  if (it != m_alloc_sites_bimap.left.end ())
    return it->second;
  else
    return 0; // not found
}

const Value* DsaInfo::getAllocValue (unsigned int alloc_site_id) const {
  auto it = m_alloc_sites_bimap.right.find (alloc_site_id);
  if (it != m_alloc_sites_bimap.right.end ())
    return it->second;
  else
    return nullptr; //not found
}

////////////////////////////////////////////////////////////
// LLVM pass
////////////////////////////////////////////////////////////

void DsaInfoPass::getAnalysisUsage (AnalysisUsage &AU) const {
  AU.addRequired<DsaAnalysis>();
  AU.setPreservesAll ();
}

bool DsaInfoPass::runOnModule (Module &M) {

  auto &dsa = getAnalysis<DsaAnalysis>();
  m_dsa_info.reset(
      new DsaInfo(dsa.getDataLayout(), dsa.getTLI(), dsa.getDsaAnalysis()));

  m_dsa_info->runOnModule (M);
  return false;
}

DsaInfo& DsaInfoPass::getDsaInfo () {   
  assert (m_dsa_info);
  return *(static_cast<DsaInfo*> (&*m_dsa_info));
} 

Pass *createDsaInfoPass() { return new sea_dsa::DsaInfoPass(); }

char sea_dsa::DsaInfoPass::ID = 0;

static llvm::RegisterPass<sea_dsa::DsaInfoPass> 
X ("seadsa-info", "Gather info about SeaDsa memory graphs");
