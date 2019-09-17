#ifndef __DSA_INFO_HH_
#define __DSA_INFO_HH_

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/ADT/StringRef.h"

#include "boost/container/flat_set.hpp"
#include <boost/unordered_map.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <boost/bimap.hpp>

/* Precompute queries for dsa clients */

namespace llvm 
{
    class DataLayout;
    class TargetLibraryInfo;
    class Value;
    class Function;
    class raw_ostream;
}

namespace sea_dsa 
{
  class Node;
  class Graph; 
  class GlobalAnalysis;
}


namespace sea_dsa {
  
  // Wrapper to extend a dsa node with extra information
  class NodeWrapper 
  {      
    const Node* m_node; 
    unsigned m_id;
    unsigned m_accesses;
    // Name of one of the node's referrers.
    // The node is chosen deterministically 
    std::string m_rep_name;
    
  public:
    
    NodeWrapper (const Node* node, unsigned id, std::string name)
      : m_node(node), m_id(id), m_accesses(0), m_rep_name (name) {}
    
    bool operator==(const NodeWrapper&o) const  {
      // XXX: we do not want to use pointer addresses here
      return (getId() == o.getId());
    }
    
    bool operator<(const NodeWrapper&o) const  {
      // XXX: we do not want to use pointer addresses here
      return (getId() < o.getId());
    }
    
    const Node* getNode () const { return m_node; }
    unsigned getId () const { return m_id;}
    NodeWrapper& operator++ () { // prefix ++
      m_accesses++;
      return *this;
    }
    
    unsigned getAccesses () const { return m_accesses;}
  };
  
  class DsaInfo {
    
    typedef boost::unordered_map<const Node*, NodeWrapper> NodeWrapperMap;
    typedef boost::bimap<const llvm::Value*, unsigned int> AllocSiteBiMap;
    typedef boost::container::flat_set<const llvm::Value*> ValueSet;
    typedef boost::container::flat_set<unsigned int> IdSet;
    typedef boost::container::flat_set<NodeWrapper> NodeWrapperSet;
    typedef boost::container::flat_set<Graph*> GraphSet;
    typedef boost::unordered_map<const llvm::Value*, std::string> NamingMap;
    
    const llvm::DataLayout &m_dl;
    const llvm::TargetLibraryInfo &m_tli;
    GlobalAnalysis &m_dsa;
    NodeWrapperMap m_nodes_map; // map Node to NodeWrapper
    AllocSiteBiMap m_alloc_sites_bimap; // bimap allocation sites to id
    IdSet m_alloc_sites_set;
    NamingMap m_names; // map Value to string name
    GraphSet m_seen_graphs;

    
    typedef typename NodeWrapperMap::value_type binding_t;
    struct get_second : public std::unary_function<binding_t, NodeWrapper> 
    { const NodeWrapper& operator()(const binding_t &kv) const {
	return kv.second;
      }
    };
    
    typedef boost::transform_iterator<get_second,
				      typename NodeWrapperMap::const_iterator>
    nodes_const_iterator;
    typedef boost::iterator_range<nodes_const_iterator> nodes_const_range;
        
    nodes_const_iterator nodes_begin () const {
      return boost::make_transform_iterator(m_nodes_map.begin(), get_second());
    }
    
    nodes_const_iterator nodes_end () const {
      return boost::make_transform_iterator(m_nodes_map.end(), get_second());
    }
    
    nodes_const_range nodes () const {
      return boost::make_iterator_range(nodes_begin (), nodes_end());
    }

    struct is_alive_node { bool operator()(const NodeWrapper&);};
    typedef boost::filter_iterator<is_alive_node, nodes_const_iterator>
    live_nodes_const_iterator;

  public:
    
    typedef boost::iterator_range<live_nodes_const_iterator> live_nodes_const_range;
    typedef IdSet alloc_sites_set;
    
  private:
    
    live_nodes_const_iterator live_nodes_begin () const {
      return boost::make_filter_iterator (is_alive_node(), nodes_begin());
    }
    
    live_nodes_const_iterator live_nodes_end () const {
      return boost::make_filter_iterator (is_alive_node(), nodes_end());
    }
    
    
    std::string getName (const llvm::Function &fn, const llvm::Value& v);       
    
    void recordMemAccess (const llvm::Value* v, Graph& g, const llvm::Instruction &I); 
    
    void recordMemAccesses (const llvm::Function& f);
    
    void assignNodeId (const llvm::Function &fn, Graph* g);

    bool recordAllocSite (const llvm::Value* v, unsigned &site_id);
    
    void assignAllocSiteId ();
        
  public:
    
    DsaInfo (const llvm::DataLayout &dl,
	     const llvm::TargetLibraryInfo &tli,
	     GlobalAnalysis &dsa,
	     bool verbose = true)
      : m_dl (dl), m_tli (tli), m_dsa (dsa) {}
    
    bool runOnModule (llvm::Module &M);
    bool runOnFunction (llvm::Function &fn);

    // Iterate over all non-trival Dsa nodes
    live_nodes_const_range live_nodes () const {
      return boost::make_iterator_range (live_nodes_begin (),
					 live_nodes_end());
    }

    // Return the set of all allocation sites
    const alloc_sites_set& alloc_sites () const {
      return m_alloc_sites_set;
    }
    
    ////////
    /// API for Dsa clients
    ////////
    
    Graph* getDsaGraph (const llvm::Function& f) const;
    
    bool isAccessed (const Node&n) const; 
    
    // return unique numeric identifier for node n if found,
    // otherwise 0
    unsigned int getDsaNodeId (const Node&n) const;
    
    // return unique numeric identifier for Value if it is an
    // allocation site, otherwise 0.
    unsigned int getAllocSiteId (const llvm::Value* V) const;
    
    // the inverse of getAllocSiteID
    const llvm::Value* getAllocValue (unsigned int alloc_site_id) const;
  };


  class DsaInfoPass: public llvm::ModulePass {
    std::unique_ptr<DsaInfo> m_dsa_info;
    
  public:
    
    static char ID;
    
    DsaInfoPass (): ModulePass (ID), m_dsa_info (nullptr) {}

    void getAnalysisUsage (llvm::AnalysisUsage &AU) const override;
    
    bool runOnModule (llvm::Module &M) override;
    
    llvm::StringRef getPassName() const override 
    { return "Extract stats from SeaDsa analysis"; }
    
    DsaInfo& getDsaInfo ();
  };
  
}
#endif 
