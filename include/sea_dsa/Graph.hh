#pragma once

#include "boost/container/flat_map.hpp"
#include "boost/container/flat_set.hpp"
#include "boost/functional/hash.hpp"
#include "boost/iterator/filter_iterator.hpp"
#include "boost/iterator/indirect_iterator.hpp"

// llvm 3.8: forward declarations not enough
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ImmutableSet.h"

#include "sea_dsa/AllocSite.hh"
#include "sea_dsa/FieldType.hh"

#include <functional>

namespace llvm {
class Type;
class DataLayout;
class raw_ostream;
} // namespace llvm

namespace sea_dsa {

class Node;
class Cell;
class Graph;
class SimulationMapper;
typedef std::unique_ptr<Cell> CellRef;

class DsaCallSite;
class DsaAllocator;
extern bool IsTypeAware;

// Data structure graph traversal iterator
template <typename T> class NodeIterator;

struct DsaAllocatorDeleter {
  DsaAllocator *m_allocator;
  DsaAllocatorDeleter(DsaAllocator &allocator) : m_allocator(&allocator) {}
  DsaAllocatorDeleter(const DsaAllocatorDeleter &o) = default;
  DsaAllocatorDeleter &operator=(const DsaAllocatorDeleter &o) = default;
  void operator()(void *block);
};

class Graph {
  friend class Node;

public:
  typedef llvm::ImmutableSet<llvm::Type *> Set;
  typedef typename Set::Factory SetFactory;

protected:
  const llvm::DataLayout &m_dl;
  SetFactory &m_setFactory;

  std::unique_ptr<DsaAllocator> m_allocator;
  /// DSA nodes owned by this graph
  typedef std::vector<std::unique_ptr<Node, DsaAllocatorDeleter>> NodeVector;
  typedef std::unique_ptr<Node, DsaAllocatorDeleter> NodeVectorElemTy;
  NodeVector m_nodes;

  /// Map from scalars to cells in this graph
  typedef llvm::DenseMap<const llvm::Value *, CellRef> ValueMap;
  ValueMap m_values;

  /// Map from formal arguments to cells
  typedef llvm::DenseMap<const llvm::Argument *, CellRef> ArgumentMap;
  ArgumentMap m_formals;

  /// Map from formal returns of functions to cells
  typedef llvm::DenseMap<const llvm::Function *, CellRef> ReturnMap;
  ReturnMap m_returns;

  using AllocSites = std::vector<std::unique_ptr<DsaAllocSite>>;
  AllocSites m_allocSites;

  using ValueToAllocSite = llvm::DenseMap<const llvm::Value *, DsaAllocSite *>;
  ValueToAllocSite m_valueToAllocSite;

  /// Indirect call sites owned by this graph
  ///
  /// The call site can be defined in the current function or any
  /// direct or indirect callee. Call sites are copied from callees to
  /// callers during bottom-up propagation.
  using CallSites = std::vector<std::unique_ptr<DsaCallSite>>;
  CallSites m_callSites;

  /// Map from instructions to call sites
  using InstructionToCallSite = llvm::DenseMap<const llvm::Instruction *, DsaCallSite *>;
  InstructionToCallSite m_instructionToCallSite;

  //  Whether the graph is flat or not
  bool m_is_flat;

  SetFactory &getSetFactory() { return m_setFactory; }
  Set emptySet() { return m_setFactory.getEmptySet(); }
  /// return a new set that is the union of old and a set containing v
  Set mkSet(Set old, const llvm::Type *v) { return m_setFactory.add(old, v); }

  const llvm::DataLayout &getDataLayout() const { return m_dl; }

  struct IsGlobal {
    bool operator()(const ValueMap::value_type &kv) const;
  };

public:
  typedef boost::indirect_iterator<typename NodeVector::const_iterator>
      const_iterator;
  typedef boost::indirect_iterator<typename NodeVector::iterator> iterator;
  typedef ValueMap::const_iterator scalar_const_iterator;
  typedef boost::filter_iterator<IsGlobal, typename ValueMap::const_iterator>
      global_const_iterator;
  typedef ArgumentMap::const_iterator formal_const_iterator;
  typedef ReturnMap::const_iterator return_const_iterator;
  using alloc_site_iterator =
      boost::indirect_iterator<typename AllocSites::iterator>;
  using alloc_site_const_iterator =
      boost::indirect_iterator<typename AllocSites::const_iterator>;
  using callsite_iterator =
      boost::indirect_iterator<typename CallSites::iterator>;
  using callsite_const_iterator =
      boost::indirect_iterator<typename CallSites::const_iterator>;

  Graph(const llvm::DataLayout &dl, SetFactory &sf, bool is_flat = false);
  virtual ~Graph();

  /// remove all forwarding nodes
  virtual void compress();

  /// remove all dead nodes
  virtual void remove_dead();

  /// -- allocates a new node
  virtual Node &mkNode();

  virtual Node &cloneNode(const Node &n, bool cpAllocSites = true);

  /// iterate over nodes
  virtual const_iterator begin() const;
  virtual const_iterator end() const;
  virtual iterator begin();
  virtual iterator end();
  size_t numNodes() const { return m_nodes.size(); };
  size_t numCollapsed() const;

  /// iterate over scalars
  virtual scalar_const_iterator scalar_begin() const;
  virtual scalar_const_iterator scalar_end() const;
  llvm::iterator_range<scalar_const_iterator> scalars() const {
    return llvm::make_range(scalar_begin(), scalar_end());
  }

  virtual global_const_iterator globals_begin() const;
  virtual global_const_iterator globals_end() const;
  llvm::iterator_range<global_const_iterator> globals() const {
    return llvm::make_range(globals_begin(), globals_end());
  }

  /// iterate over formal parameters of functions
  virtual formal_const_iterator formal_begin() const;
  virtual formal_const_iterator formal_end() const;
  llvm::iterator_range<formal_const_iterator> formals() const {
    return llvm::make_range(formal_begin(), formal_end());
  }

  /// iterate over returns of functions
  virtual return_const_iterator return_begin() const;
  virtual return_const_iterator return_end() const;
  llvm::iterator_range<return_const_iterator> returns() const {
    return llvm::make_range(return_begin(), return_end());
  }

  /// creates a cell for the value or returns existing cell if
  /// present
  virtual Cell &mkCell(const llvm::Value &v, const Cell &c);
  virtual Cell &mkRetCell(const llvm::Function &fn, const Cell &c);

  /// return a cell for the value
  virtual const Cell &getCell(const llvm::Value &v);

  /// return true iff the value has a cell
  virtual bool hasCell(const llvm::Value &v) const;

  virtual bool hasScalarCell(const llvm::Value &v) {
    return m_values.count(&v) > 0;
  }

  virtual bool hasRetCell(const llvm::Function &fn) const {
    return m_returns.count(&fn) > 0;
  }

  virtual Cell &getRetCell(const llvm::Function &fn);

  virtual const Cell &getRetCell(const llvm::Function &fn) const;

  llvm::Optional<DsaAllocSite *> getAllocSite(const llvm::Value &v) const {
    auto it = m_valueToAllocSite.find(&v);
    if (it != m_valueToAllocSite.end())
      return it->second;

    return llvm::None;
  }

  DsaAllocSite *mkAllocSite(const llvm::Value &v);

  void clearCallSites();
  
  llvm::iterator_range<alloc_site_iterator> alloc_sites() {
    alloc_site_iterator begin = m_allocSites.begin();
    alloc_site_iterator end = m_allocSites.end();
    return llvm::make_range(begin, end);
  }

  llvm::iterator_range<alloc_site_const_iterator> alloc_sites() const {
    alloc_site_const_iterator begin = m_allocSites.begin();
    alloc_site_const_iterator end = m_allocSites.end();
    return llvm::make_range(begin, end);
  }
 
  bool hasAllocSiteForValue(const llvm::Value &v) const {
    return m_valueToAllocSite.count(&v) > 0;
  }

  // return null if no callsite found
  DsaCallSite* getCallSite(const llvm::Instruction &cs) {
    auto it = m_instructionToCallSite.find(&cs);
    if (it != m_instructionToCallSite.end()) {
      return &*it->second;
    } else {
      return nullptr;
    }
  }

  DsaCallSite* mkCallSite(const llvm::Instruction &cs, Cell c);
  
  llvm::iterator_range<callsite_iterator> callsites();

  llvm::iterator_range<callsite_const_iterator> callsites() const;
  
  /// compute a map from callee nodes to caller nodes
  //
  /// XXX: we might want to make the last argument a template
  /// parameter but then the definition should be in a header file.
  static bool
  computeCalleeCallerMapping(const DsaCallSite &cs, Graph &calleeG,
                             Graph &callerG, SimulationMapper &simMap,
                             const bool reportIfSanityCheckFailed = true);

  /// import the given graph into the current one
  /// copies all nodes from g and unifies all common scalars
  virtual void import(const Graph &g, bool withFormals = false);

  /// pretty-printer of a graph
  virtual void write(llvm::raw_ostream &o) const;

  /// for gdb
  void dump() const;

  friend void ShowDsaGraph(Graph &g);
  /// view the Dsa graph using GraphViz. (For debugging.)
  void viewGraph();

  bool isFlat() const { return m_is_flat; }

  void removeLinks(Node *n, std::function<bool(const Node *)> pred);

  void removeNodes(std::function<bool(const Node *)> pred);
};

/**
 *  Graph with one single node
 **/
class FlatGraph : public Graph {

public:
  FlatGraph(const llvm::DataLayout &dl, SetFactory &sf) : Graph(dl, sf, true) {}

  virtual Node &mkNode() override;
};

class Field {
  unsigned m_offset = -1;
  FieldType m_type = FIELD_TYPE_NOT_IMPLEMENTED;

public:
  Field() = default;
  Field(unsigned offset, FieldType type) : m_offset(offset), m_type(type) {}
  Field(const Field &) = default;
  Field &operator=(const Field &) = default;

  Field addOffset(unsigned offset) const { return {m_offset + offset, m_type}; }

  Field subOffset(unsigned offset) const { return {m_offset - offset, m_type}; }

  std::pair<unsigned, FieldType> asTuple() const { return {m_offset, m_type}; };

  unsigned getOffset() const { return m_offset; }
  FieldType getType() const { return m_type; }

  bool operator<(const Field &o) const { return asTuple() < o.asTuple(); }
  bool operator==(const Field &o) const { return asTuple() == o.asTuple(); }

  void dump(llvm::raw_ostream &os = llvm::errs()) const {
    os << "<" << m_offset << ", ";
    m_type.dump(os);
    os << ">";
  }

  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &o, const Field &f) {
    f.dump(o);
    return o;
  }
};

/**
    A memory cell (or a field). An offset into a memory object.
*/
class Cell {
  /// memory object
  mutable Node *m_node = nullptr;
  /// field offset
  mutable unsigned m_offset = 0;

  std::tuple<Node *, unsigned> asTuple() const {
    return std::make_tuple(m_node, m_offset);
  };

public:
  Cell() = default;
  Cell(const Cell &) = default;

  Cell(Node *node, unsigned offset) : m_node(node), m_offset(offset) {}

  Cell(Node &node, unsigned offset) : m_node(&node), m_offset(offset) {}

  Cell(const Cell &o, unsigned offset)
      : m_node(o.m_node), m_offset(o.m_offset + offset) {}

  Cell &operator=(const Cell &o) = default;

  bool operator==(const Cell &o) const { return asTuple() == o.asTuple(); }
  bool operator!=(const Cell &o) const { return !operator==(o); }
  bool operator<(const Cell &o) const { return asTuple() < o.asTuple(); }

  void setRead(bool v = true);
  void setModified(bool v = true);

  bool isRead() const;
  bool isModified() const;

  bool isNull() const { return m_node == nullptr; }
  Node *getNode() const;
  // for internal Dsa use (actual offset)
  unsigned getRawOffset() const;
  // for Dsa clients (offset is adjusted based on the node)
  unsigned getOffset() const;

  void pointTo(Node &n, unsigned offset);

  void pointTo(const Cell &c, unsigned offset = 0) {
    assert(!c.isNull());
    Node *n = c.getNode();
    pointTo(*n, c.getRawOffset() + offset);
  }

  inline bool hasLink(Field offset) const;
  inline const Cell &getLink(Field offset) const;
  inline void setLink(Field offset, const Cell &c);
  inline void addLink(Field offset, Cell &c);
  inline void addAccessedType(unsigned offset, llvm::Type *t);
  inline void growSize(unsigned offset, llvm::Type *t);

  /// unify with a given cell. At the end, both cells point to the
  /// same offset of the same node. Might cause collapse of the
  /// nodes represented by the cells.
  void unify(Cell &c);

  void swap(Cell &o) {
    std::swap(m_node, o.m_node);
    std::swap(m_offset, o.m_offset);
  }

  /// pretty-printer of a cell
  void write(llvm::raw_ostream &o) const;

  /// for gdb
  void dump() const;
};

/// A node of a DSA graph representing a memory object
class Node {
  friend class Graph;
  friend class FlatGraph;
  friend class Cell;

  friend class FunctionalMapper;
  friend class SimulationMapper;

public:
  struct NodeType {
    unsigned shadow : 1;
    unsigned foreign : 1; // for internal use
    unsigned alloca : 1;
    unsigned heap : 1;
    unsigned global : 1;
    unsigned externFunc : 1;
    unsigned externGlobal : 1;
    unsigned unknown : 1;
    unsigned incomplete : 1;
    unsigned modified : 1;
    unsigned read : 1;
    unsigned array : 1;
    unsigned offset_collapsed : 1;
    unsigned type_collapsed : 1;
    unsigned external : 1;
    unsigned inttoptr : 1;
    unsigned ptrtoint : 1;
    unsigned vastart : 1;
    unsigned dead : 1;
    unsigned null : 1;

    NodeType() { reset(); }

    void join(const NodeType &n) {
      shadow |= n.shadow;
      foreign &= n.foreign;
      alloca |= n.alloca;
      heap |= n.heap;
      global |= n.global;
      externFunc |= n.externFunc;
      externGlobal |= n.externGlobal;
      unknown |= n.unknown;
      incomplete |= n.incomplete;
      modified |= n.modified;
      read |= n.read;
      array |= n.array;
      offset_collapsed |= n.offset_collapsed;
      type_collapsed |= n.type_collapsed;
      external |= n.external;
      inttoptr |= n.inttoptr;
      ptrtoint |= n.ptrtoint;
      vastart |= n.vastart;
      dead |= n.dead;
      null |= n.null;

      // XXX: cannot be offset-collapsed and array at the same time
      if (offset_collapsed && array)
        array = 0;
    }
    void reset() { memset(this, 0, sizeof(*this)); }

    std::string toStr() const {
      std::string flags;

      if (offset_collapsed)
        flags += "oC";
      if (type_collapsed)
        flags += "tC";
      if (alloca)
        flags += "S";
      if (heap)
        flags += "H";
      if (global)
        flags += "G";
      if (array)
        flags += "A";
      if (unknown)
        flags += "U";
      if (incomplete)
        flags += "I";
      if (modified)
        flags += "M";
      if (read)
        flags += "R";
      if (external)
        flags += "E";
      if (externFunc)
        flags += "X";
      if (externGlobal)
        flags += "Y";
      if (inttoptr)
        flags += "P";
      if (ptrtoint)
        flags += "2";
      if (vastart)
        flags += "V";
      if (dead)
        flags += "D";
      if (null)
        flags += "N";
      return flags;
    }
  };

private:
  /// parent DSA graph
  Graph *m_graph;
  /// node marks
  struct NodeType m_nodeType;

  mutable const llvm::Value *m_unique_scalar;
  // True if had a unique scalar at some point
  bool m_has_once_unique_scalar;
  /// When the node is forwarding, the memory cell at which the
  /// node begins in some other memory object
  Cell m_forward;

public:
  typedef Graph::Set Set;
  // TODO: Investigate why flat_map is slower for accessed_types_type.
  typedef llvm::DenseMap<unsigned, Set> accessed_types_type;
  typedef boost::container::flat_map<Field, CellRef> links_type;

  // Iterator for graph interface... Defined in GraphTraits.h
  typedef NodeIterator<Node> iterator;
  typedef NodeIterator<const Node> const_iterator;
  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  NodeType getNodeType() const { return m_nodeType; }

  const links_type &getLinks() const { return m_links; }

private:
  links_type &getLinks() { return m_links; }

protected:
  class Offset;
  friend class Offset;
  /// helper class to ensure that offsets are properly adjusted
  class Offset {
    const Node &m_node;
    const unsigned m_offset;

  public:
    Offset(const Node &n, unsigned offset) : m_node(n), m_offset(offset) {}

    unsigned getNumericOffset() const;

    Field getAdjustedField(Field f) {
      return Field(getNumericOffset(), f.getType());
    }

    static Field getAdjustedField(const Node &n, Field f) {
      return Offset(n, f.getOffset()).getAdjustedField(f);
    }

    const Node &node() const { return m_node; }
  };

private:
  /// known type of every offset/field
  accessed_types_type m_accessedTypes;
  /// destination of every offset/field
  links_type m_links;

  /// known size
  unsigned m_size;

  /// allocation sites for the node
  typedef boost::container::flat_set<const llvm::Value *> AllocaSet;
  AllocaSet m_alloca_sites;

  /// XXX This is ugly. Ids should probably be unique per-graph, not
  /// XXX unique overall. Check with @jnavas before changing this though...
  static uint64_t m_id_factory;

  uint64_t m_id; // global id for the node

  Node(Graph &g);
  Node(Graph &g, const Node &n, bool cpLinks = false, bool cpAllocSites = true);

  void compress() {
    constexpr unsigned shrinkThreshold = 4;
    if (m_accessedTypes.size() * shrinkThreshold <
        m_accessedTypes.getMemorySize())
      m_accessedTypes =
          accessed_types_type(m_accessedTypes.begin(), m_accessedTypes.end());

    if (m_links.size() * shrinkThreshold < m_links.capacity())
      m_links.shrink_to_fit();

    if (m_alloca_sites.size() * shrinkThreshold < m_alloca_sites.capacity())
      m_alloca_sites.shrink_to_fit();
  }
  /// Transfer links/types and other information from the current
  /// node to the given one at a given offset and make the current
  /// one point to the result. Might cause collapse.  Most clients
  /// should use unifyAt() that has less stringent preconditions.
  void pointTo(Node &node, const Offset &offset);

  Cell &getLink(const Field &f) {
    auto &res = m_links[Offset::getAdjustedField(*this, f)];
    if (!res)
      res.reset(new Cell());
    return *res;
  }

  /// Adds a set of types for a field at a given offset
  void addAccessedType(const Offset &offset, Set types);

  /// joins all the types of a given node starting at a given
  /// offset of the current node
  void joinAccessedTypes(unsigned offset, const Node &n);

  /// increase size to accommodate a field of type t at the given offset
  void growSize(const Offset &offset, const llvm::Type *t);
  Node &setArray(bool v = true) {
    m_nodeType.array = v;
    return *this;
  }

  void writeAccessedTypes(llvm::raw_ostream &o) const;

public:
  /// delete copy constructor
  Node(const Node &n) = delete;
  /// delete assignment
  Node &operator=(const Node &n) = delete;

  /// unify with a given node
  void unify(Node &n) { unifyAt(n, 0); }

  /// Unify a given node with a specified offset of the current node
  /// post-condition: the given node points to the current node.
  /// might cause a collapse
  void unifyAt(Node &n, unsigned offset);

  Node &setAlloca(bool v = true) {
    m_nodeType.alloca = v;
    return *this;
  }
  Node &setHeap(bool v = true) {
    m_nodeType.heap = v;
    return *this;
  }
  Node &setRead(bool v = true) {
    m_nodeType.read = v;
    return *this;
  }
  Node &setModified(bool v = true) {
    m_nodeType.modified = v;
    return *this;
  }
  Node &setExternal(bool v = true) {
    m_nodeType.external = v;
    return *this;
  }
  Node &setIntToPtr(bool v = true) {
    m_nodeType.inttoptr = v;
    return *this;
  }
  Node &setPtrToInt(bool v = true) {
    m_nodeType.ptrtoint = v;
    return *this;
  }

  bool isAlloca() const { return m_nodeType.alloca; }
  bool isHeap() const { return m_nodeType.heap; }
  bool isRead() const { return m_nodeType.read; }
  bool isModified() const { return m_nodeType.modified; }
  bool isExternal() const { return m_nodeType.external; }
  bool isIntToPtr() const { return m_nodeType.inttoptr; }
  bool isPtrToInt() const { return m_nodeType.ptrtoint; }
  bool isIncomplete() const { return m_nodeType.incomplete; }
  bool isUnknown() const { return m_nodeType.unknown; }


  Node &setArraySize(unsigned sz) {
    assert(!isArray());
    assert(!isForwarding());
    assert(m_size <= sz);

    setArray(true);
    m_size = sz;
    return *this;
  }

  bool isArray() const { return m_nodeType.array; }

  Node &setOffsetCollapsed(bool v = true) {
    m_nodeType.offset_collapsed = v;
    setArray(false);
    return *this;
  }
  bool isOffsetCollapsed() const { return m_nodeType.offset_collapsed; }

  Node &setTypeCollapsed(bool v = true) {
    m_nodeType.type_collapsed = v;
    return *this;
  }
  bool isTypeCollapsed() const { return m_nodeType.type_collapsed; }

  bool isForeign() const { return m_nodeType.foreign; }

  Node &setForeign(bool v = true) {
    m_nodeType.foreign = v;
    return *this;
  }

  bool isUnique() const { return m_unique_scalar; }
  const llvm::Value *getUniqueScalar() const { return m_unique_scalar; }
  void setUniqueScalar(const llvm::Value *v) {
    m_unique_scalar = v;
    m_has_once_unique_scalar = true;
  }
  bool hasOnceUniqueScalar() const { return m_has_once_unique_scalar; }
  /// compute a simulation relation between this and n while
  /// propagating unique scalars for all the nodes in the
  /// relation. The result can only be one of these four values:
  /// 0x0 (no change), 0x1 (this changed), 0x2 (n changed), 0x3
  /// (both changed).
  unsigned mergeUniqueScalar(Node &n);
  template <typename Cache> unsigned mergeUniqueScalar(Node &n, Cache &seen);

  inline bool isForwarding() const;
  inline Cell &getForwardDest();
  inline const Cell &getForwardDest() const;

  // global id for the node
  uint64_t getId() const { return m_id; }

  Graph *getGraph() { return m_graph; }
  const Graph *getGraph() const { return m_graph; }

  /// Return a node the current node represents. If the node is
  /// forwarding, returns the non-forwarding node this node points
  /// to. Might be expensive.
  inline Node *getNode();
  inline const Node *getNode() const;
  unsigned getRawOffset() const;

  accessed_types_type &types() { return m_accessedTypes; }
  const accessed_types_type &types() const { return m_accessedTypes; }
  links_type &links() { return m_links; }
  const links_type &links() const { return m_links; }

  unsigned size() const { return m_size; }
  void growSize(unsigned v);

  bool hasLink(Field f) const {
    if (!IsTypeAware)
      assert(f.getType().isUnknown());
    return m_links.count(Offset::getAdjustedField(*this, f)) > 0;
  }

  bool getNumLinks() const { return m_links.size(); }

  const Cell &getLink(Field f) const {
    if (!IsTypeAware)
      assert(f.getType().isUnknown());
    return *m_links.at(Offset::getAdjustedField(*this, f));
  }

  void setLink(Field f, const Cell &c) {
    getLink(Offset::getAdjustedField(*this, f)) = c;
  }

  void addLink(Field field, Cell &c);

  bool hasAccessedType(unsigned offset) const;

  const Set getAccessedType(unsigned o) const {
    Offset offset(*this, o);
    auto it = m_accessedTypes.find(offset.getNumericOffset());
    assert(it != m_accessedTypes.end());
    return it->second;
  }

  bool isVoid() const { return m_accessedTypes.empty(); }
  bool isEmtpyAccessedType() const;

  /// Nodes that originate from operations on nullptr, e.g. gep(null, Offset).
  bool isNullAlloc() const { return m_nodeType.null; }
  Node &setNullAlloc(bool v = true) {
    m_nodeType.null = v;
    return *this;
  }

  /// Adds a type of a field at a given offset
  void addAccessedType(unsigned offset, llvm::Type *t);

  /// collapse the current node. Looses all offset-based field sensitivity
  /// tag argument is used for debugging only
  void collapseOffsets(int tag /*= -2*/);

  /// collapse the current node. Looses all type-based field sensitivity
  /// tag argument is used for debugging only
  void collapseTypes(int tag /*= -2*/);

  /// Add a new allocation site
  void addAllocSite(const DsaAllocSite &v);
  /// get all allocation sites
  const AllocaSet &getAllocSites() const { return m_alloca_sites; }

  void resetAllocSites() { m_alloca_sites.clear(); }

  template <typename Iterator>
  void insertAllocSites(Iterator begin, Iterator end) {
    m_alloca_sites.insert(begin, end);
  }

  bool hasAllocSite(const llvm::Value &v) { return m_alloca_sites.count(&v); }

  /// joins all the allocation sites
  void joinAllocSites(const AllocaSet &s);
  /// compute a simulation relation between this and n while
  /// propagating allocation sites for all the nodes in the
  /// relation. The result can only be one of these four values:
  /// 0x0 (no change), 0x1 (this changed), 0x2 (n changed), 0x3
  /// (both changed).
  unsigned mergeAllocSites(Node &n);
  template <typename Cache> unsigned mergeAllocSites(Node &n, Cache &seen);

  /// pretty-printer of a node
  void write(llvm::raw_ostream &o) const;

  /// for gdb
  void dump() const;

  // Shows the Dsa graph using GraphViz. (For debugging.)
  void viewGraph();
};

bool Node::isForwarding() const { return !m_forward.isNull(); }

Cell &Node::getForwardDest() { return m_forward; }
const Cell &Node::getForwardDest() const { return m_forward; }

bool Cell::hasLink(Field offset) const {
  return m_node && getNode()->hasLink(offset.addOffset(m_offset));
}

const Cell &Cell::getLink(Field offset) const {
  assert(m_node);
  return getNode()->getLink(offset.addOffset(m_offset));
}

void Cell::setLink(Field offset, const Cell &c) {
  getNode()->setLink(offset.addOffset(m_offset), c);
}

void Cell::addLink(Field offset, Cell &c) {
  getNode()->addLink(offset.addOffset(m_offset), c);
}

void Cell::addAccessedType(unsigned offset, llvm::Type *t) {
  getNode()->addAccessedType(m_offset + offset, t);
}

void Cell::growSize(unsigned o, llvm::Type *t) {
  assert(!isNull());
  Node::Offset offset(*getNode(), m_offset + o);
  getNode()->growSize(offset, t);
}

Node *Node::getNode() { return isForwarding() ? m_forward.getNode() : this; }

const Node *Node::getNode() const {
  return isForwarding() ? m_forward.getNode() : this;
}
} // namespace sea_dsa

namespace llvm {
inline raw_ostream &operator<<(raw_ostream &o, const sea_dsa::Node &n) {
  n.write(o);
  return o;
}
inline raw_ostream &operator<<(raw_ostream &o, const sea_dsa::Cell &c) {
  c.write(o);
  return o;
}
} // namespace llvm

namespace std {
template <> struct hash<sea_dsa::Cell> {
  size_t operator()(const sea_dsa::Cell &c) const {
    size_t seed = 0;
    boost::hash_combine(seed, c.getNode());
    boost::hash_combine(seed, c.getRawOffset());
    // boost::hash_combine(seed, c.getType().asTuple());
    return seed;
  }
};
} // namespace std

