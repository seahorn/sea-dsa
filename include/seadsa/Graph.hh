#pragma once

#include "boost/container/flat_map.hpp"
#include "boost/container/flat_set.hpp"
#include "boost/functional/hash.hpp"
#include "boost/iterator/filter_iterator.hpp"
#include "boost/iterator/indirect_iterator.hpp"
#include "boost/optional/optional.hpp"

// llvm 3.8: forward declarations not enough
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Value.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ImmutableSet.h"

#include "seadsa/AllocSite.hh"
#include "seadsa/FieldType.hh"

#include <functional>
#include <optional>

namespace llvm {
class Type;
class DataLayout;
class raw_ostream;
} // namespace llvm

namespace seadsa {

class Node;
class Cell;
class Graph;
class SimulationMapper;
using CellRef = std::unique_ptr<Cell>;

class DsaCallSite;
class DsaAllocator;
extern bool g_IsTypeAware;
/// Global flag controlling whether DSA uses partial offset collapse
extern bool g_IsPartialCollapseEnabled;

/// True iff partial (interval) collapse is enabled AND usable: interval
/// cells key fields at type granularity, so the feature relies on
/// --sea-dsa-type-aware. If --sea-dsa-partial-collapse is set without it,
/// this warns once and reports false (partial collapse stays off).
bool IsPartialCollapseActive();

/// Data structure graph traversal iterator
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
  using Set = llvm::ImmutableSet<llvm::Type *>;
  using SetFactory = typename Set::Factory;

protected:
  const llvm::DataLayout &m_dl;
  SetFactory &m_setFactory;

  std::unique_ptr<DsaAllocator> m_allocator;
  /// DSA nodes owned by this graph
  using NodeVector = std::vector<std::unique_ptr<Node, DsaAllocatorDeleter>>;
  using NodeVectorElemTy = std::unique_ptr<Node, DsaAllocatorDeleter>;
  NodeVector m_nodes;

  /// Map from scalars to cells in this graph
  using ValueMap = llvm::DenseMap<const llvm::Value *, CellRef>;
  ValueMap m_values;

  /// Map from formal arguments to cells
  using ArgumentMap = llvm::DenseMap<const llvm::Argument *, CellRef>;
  ArgumentMap m_formals;

  /// Map from formal returns of functions to cells
  using ReturnMap = llvm::DenseMap<const llvm::Function *, CellRef>;
  ReturnMap m_returns;

  using AllocSites = std::vector<std::unique_ptr<DsaAllocSite>>;
  AllocSites m_allocSites;

  using ValueToAllocSite = llvm::DenseMap<const llvm::Value *, DsaAllocSite *>;
  ValueToAllocSite m_valueToAllocSite;

  using CallSites =
      std::vector<std::unique_ptr<DsaCallSite>>; /// Indirect call sites owned
                                                 /// by this graph
  ///
  /// The call site can be defined in the current function or any
  /// direct or indirect callee. Call sites are copied from callees to
  /// callers during bottom-up propagation.
  CallSites m_callSites;

  /// Map from instructions to call sites
  using InstructionToCallSite =
      llvm::DenseMap<const llvm::Instruction *, DsaCallSite *>;
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
  using const_iterator =
      boost::indirect_iterator<typename NodeVector::const_iterator>;
  using iterator = boost::indirect_iterator<typename NodeVector::iterator>;
  using scalar_const_iterator = ValueMap::const_iterator;
  using global_const_iterator =
      boost::filter_iterator<IsGlobal, typename ValueMap::const_iterator>;
  using formal_const_iterator = ArgumentMap::const_iterator;
  using return_const_iterator = ReturnMap::const_iterator;
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

  std::optional<DsaAllocSite *> getAllocSite(const llvm::Value &v) const {
    auto it = m_valueToAllocSite.find(&v);
    if (it != m_valueToAllocSite.end()) return it->second;

    return std::nullopt;
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
  DsaCallSite *getCallSite(const llvm::Instruction &cs) {
    auto it = m_instructionToCallSite.find(&cs);
    if (it != m_instructionToCallSite.end()) {
      return &*it->second;
    } else {
      return nullptr;
    }
  }

  DsaCallSite *mkCallSite(const llvm::Instruction &cs, Cell c);

  llvm::iterator_range<callsite_iterator> callsites();

  llvm::iterator_range<callsite_const_iterator> callsites() const;

  /// Compute a simulation relation map from callee nodes to caller
  /// nodes.
  static bool
  computeCalleeCallerMapping(const DsaCallSite &cs, Graph &calleeG,
                             Graph &callerG, SimulationMapper &simMap,
                             const bool reportIfSanityCheckFailed = true);

  /// Compute a simulation relation between two arbitrary function's
  /// graphs.  Return true if the graph fromG is simulated by the
  /// graph toG.
  static bool computeSimulationMapping(Graph &fromG, Graph &toG,
                                       SimulationMapper &simMap,
                                       bool onlyModified = false);

  /// import the given graph into the current one
  /// copies all nodes from g and unifies all common scalars
  virtual void import(const Graph &g, bool withFormals = false);

  /// pretty-printer of a graph
  virtual void write(llvm::raw_ostream &o) const;

  /// for gdb
  void dump() const;

  friend void WriteDsaGraph(Graph &g, const std::string &filename);
  /// write the Dsa graph in dot format
  void writeGraph(const std::string &filename);

  friend void ShowDsaGraph(Graph &g);
  /// view the Dsa graph using GraphViz. (For debugging.)
  void viewGraph();

  bool isFlat() const { return m_is_flat; }

  void removeLinks(Node *n, std::function<bool(const Node *)> pred);

  void removeNodes(std::function<bool(const Node *)> pred);
};

/**
 * @class FlatGraph
 * @brief A graph with a single collapsed node (field-insensitive)
 *
 * FlatGraph represents the most conservative DSA analysis where all
 * memory is collapsed into a single node. This loses all field sensitivity
 * but is sound and fast. Useful for baseline comparisons or when precision
 * is not required.
 */
class FlatGraph : public Graph {

public:
  FlatGraph(const llvm::DataLayout &dl, SetFactory &sf) : Graph(dl, sf, true) {}

  virtual Node &mkNode() override;
};

class Field {
  unsigned m_offset = -1;
  FieldType m_type = FIELD_TYPE_NOT_IMPLEMENTED;

  constexpr std::tuple<unsigned, FieldType> asTuple() const {
    return {m_offset, m_type};
  };

public:
  Field() = default;
  Field(unsigned offset, FieldType type) : m_offset(offset), m_type(type) {}
  Field(const Field &) = default;
  Field &operator=(const Field &) = default;

  Field addOffset(unsigned offset) const { return {m_offset + offset, m_type}; }

  Field subOffset(unsigned offset) const { return {m_offset - offset, m_type}; }

  unsigned getOffset() const { return m_offset; }
  FieldType getType() const { return m_type; }
  bool hasOmniType() const { return m_type.isOmniType(); }

  Field mkOmniField() const { return Field(m_offset, FieldType::mkOmniType()); }

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
 * @class Cell
 * @brief A memory cell representing an interval into a DSA node
 *
 * A Cell is essentially a triple (Node*, start, end) that references a
 * location or an interval within a memory object. A singleton cell at offset
 * N is represented as [N, N]. An absent end represents an infinite interval.
 * Cells are the fundamental unit for tracking data flow and aliasing in DSA.
 *
 * Cells can be:
 * - Null (pointing to no node)
 * - Direct (pointing to a non-forwarding node)
 * - Indirect (pointing to a forwarding node, resolved on access)
 */
class Cell {
protected:
  mutable Node *m_node =
      nullptr; ///< The memory object (node) this cell refers to
  mutable unsigned m_offset =
      0; ///< Byte offset within the node, start of the interval
  mutable boost::optional<unsigned> m_end =
      0; ///< Inclusive interval end, none means infinity

  std::tuple<Node *, unsigned, boost::optional<unsigned>> asTuple() const {
    return std::make_tuple(m_node, m_offset, m_end);
  };

public:
  Cell() = default;
  Cell(const Cell &) = default;

  /**
   * @brief Construct a cell pointing to a node at given offset
   * @param node Pointer to the node
   * @param offset Byte offset into the node
   */
  Cell(Node *node, unsigned offset)
      : m_node(node), m_offset(offset), m_end(offset) {}
  Cell(Node *node, unsigned start, boost::optional<unsigned> end)
      : m_node(node), m_offset(start), m_end(end) {}

  /**
   * @brief Construct a cell pointing to a node at given offset
   * @param node Reference to the node
   * @param offset Byte offset into the node
   */
  Cell(Node &node, unsigned offset)
      : m_node(&node), m_offset(offset), m_end(offset) {}
  Cell(Node &node, unsigned start, boost::optional<unsigned> end)
      : m_node(&node), m_offset(start), m_end(end) {}

  /**
   * @brief Construct a cell from another cell with additional offset
   * @param o Base cell
   * @param offset Additional offset to add
   */
  Cell(const Cell &o, unsigned offset)
      : m_node(o.m_node), m_offset(o.m_offset + offset) {
    if (o.m_end)
      m_end = o.m_end.get() + offset;
    else
      m_end = boost::none;
  }

  Cell &operator=(const Cell &o) = default;

  ~Cell() = default;

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

  /// @brief Get the raw inclusive interval end, if finite.
  boost::optional<unsigned> getRawEndOffset() const;

  /// @brief Get the adjusted inclusive interval end, if finite.
  boost::optional<unsigned> getEndOffset() const;

  /// Compatibility aliases for existing graph clients.
  unsigned getRawStartOffset() const { return getRawOffset(); }
  unsigned getStartOffset() const;

  /// @brief True if the interval contains the given offset.
  bool includes(unsigned o) const {
    return m_offset <= o && (!m_end || o <= m_end.get());
  }

  /// @brief True if this interval contains the whole interval of another cell.
  bool includes(const Cell &o) const {
    if (m_node != o.m_node || o.m_offset < m_offset) return false;
    if (!m_end) return true;
    return o.m_end && o.m_end.get() <= m_end.get();
  }

  /// @brief True if two interval cells do not overlap.
  bool isDisjoint(const Cell &o) const {
    if (m_node != o.m_node) return true;
    if (!m_end && !o.m_end) return false;
    if (!m_end) return o.m_end && o.m_end.get() < m_offset;
    if (!o.m_end) return m_end.get() < o.m_offset;
    return m_end.get() < o.m_offset || o.m_end.get() < m_offset;
  }

  /// @brief Widen this interval to include another interval on the same node.
  void mergeIntervalInPlace(const Cell &o) {
    assert(m_node == o.m_node);
    if (o.m_offset < m_offset) m_offset = o.m_offset;
    if (!m_end || !o.m_end) {
      m_end = boost::none;
    } else if (o.m_end.get() > m_end.get()) {
      m_end = o.m_end;
    }
  }

  /**
   * @brief Make this cell point to a node at given offset
   * @param n The target node
   * @param offset Byte offset into the target node
   */
  void pointTo(Node &n, unsigned offset);

  /**
   * @brief Make this cell point to a node interval.
   * @param n The target node
   * @param start Start byte offset into the target node
   * @param end Inclusive end byte offset, none means infinity
   */
  void pointTo(Node &n, unsigned start, boost::optional<unsigned> end);

  /**
   * @brief Make this cell point to the same location as another cell
   * @param c The target cell
   * @param offset Additional offset to add
   */
  void pointTo(const Cell &c, unsigned offset = 0) {
    assert(!c.isNull());
    Node *n = c.getNode();
    auto end = c.getRawEndOffset();
    if (end) end = end.get() + offset;
    pointTo(*n, c.getRawOffset() + offset, end);
  }

  inline bool hasLink(Field offset) const;
  inline const Cell &getLink(Field offset) const;
  inline void setLink(Field offset, const Cell &c);
  inline void addLink(Field offset, const Cell &c);
  inline void addAccessedType(unsigned offset, llvm::Type *t);
  inline void growSize(unsigned offset, llvm::Type *t);

  /// unify with a given cell. At the end, both cells point to the
  /// same offset of the same node. Might cause collapse of the
  /// nodes represented by the cells.
  void unify(Cell &c);

  void swap(Cell &o) {
    std::swap(m_node, o.m_node);
    std::swap(m_offset, o.m_offset);
    std::swap(m_end, o.m_end);
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
      if (offset_collapsed && array) array = 0;
    }
    void reset() { memset(this, 0, sizeof(*this)); }

    std::string toStr() const {
      std::string flags;

      if (offset_collapsed) flags += "oC";
      if (type_collapsed) flags += "tC";
      if (alloca) flags += "S";
      if (heap) flags += "H";
      if (global) flags += "G";
      if (array) flags += "A";
      if (unknown) flags += "U";
      if (incomplete) flags += "I";
      if (modified) flags += "M";
      if (read) flags += "R";
      if (external) flags += "E";
      if (externFunc) flags += "X";
      if (externGlobal) flags += "Y";
      if (inttoptr) flags += "P";
      if (ptrtoint) flags += "2";
      if (vastart) flags += "V";
      if (dead) flags += "D";
      if (null) flags += "N";
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
  using Set = Graph::Set;
  // TODO: Investigate why flat_map is slower for accessed_types_type.
  using accessed_types_type = llvm::DenseMap<unsigned, Set>;
  using links_type = boost::container::flat_map<Field, CellRef>;
  /// Set of collapsed interval cells.
  using collapsed_cells_type = boost::container::flat_set<Cell>;

  // Iterator for graph interface... Defined in GraphTraits.h
  using iterator = NodeIterator<Node>;
  using const_iterator = NodeIterator<const Node>;
  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

  NodeType getNodeType() const { return m_nodeType; }

  const links_type &getLinks() const { return m_links; }

  const collapsed_cells_type &getCollapsedCells() const {
    return m_collapsedCells;
  }
  const collapsed_cells_type &getChunks() const { return getCollapsedCells(); }

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
  accessed_types_type
      m_accessedTypes; ///< Map from offset to types accessed at that offset
  links_type m_links;  ///< Map from field to cells pointed to by that field
  /// Set of collapsed interval cells in this node.
  ///
  /// BU/TD invariant: Graph::import and the Cloner rebuild cells with
  /// singleton offsets, so per-cell interval WIDTHS are intraprocedural;
  /// only this node-level interval set survives cloning. Offset
  /// canonicalization (Offset::getNumericOffset) therefore remains the
  /// sole interprocedural consumer of these intervals.
  collapsed_cells_type m_collapsedCells;

  unsigned m_size; ///< Size of the memory object in bytes; array stride for
                   ///< array nodes
  boost::optional<unsigned>
      m_arrayMaxSize; ///< Known upper bound for array extent in bytes

  /// allocation sites for the node
  using AllocaSet = boost::container::flat_set<const llvm::Value *>;
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

  void joinCollapsedCells(const Node &node, const Offset &offset);

  /**
   * @brief Partially collapse offsets in a given range
   *
   * Merges all fields in [start, end] into a single one.
   *
   * @param start Start byte offset (inclusive)
   * @param end End byte offset (inclusive)
   * @param tag Debug tag for tracking collapse reasons
   */
  void partialCollapseOffsets(unsigned start, boost::optional<unsigned> end,
                              int tag /*= -2*/);

  /**
   * @brief Merge c into m_collapsedCells maintaining disjoint intervals
   * @param c The interval cell to be added
   */
  void mergeCollapsedCellIntoSet(Cell &c);

  /// True iff the collapsed-cell set denotes a fully collapsed node
  /// (exactly one interval, starting at 0, unbounded).
  bool areCollapsedCellsShownCollapsed() const;

  Cell &getLink_(const Field &_f);

  /// Adds a set of types for a field at a given offset
  void addAccessedType(const Offset &offset, Set types);

  /// joins all the types of a given node starting at a given
  /// offset of the current node
  void joinAccessedTypes(unsigned offset, const Node &n);

  /// increase size to accommodate a field of type t at the given offset
  void growSize(const Offset &offset, const llvm::Type *t);
  Node &setArray(bool v = true) {
    m_nodeType.array = v;
    if (!v) {
      m_arrayMaxSize = boost::none;
    }
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

  Node &setArraySize(unsigned sz) { return setArraySize(sz, boost::none); }

  /**
   * @brief Set this node as an array with a stride and an optional extent
   *
   * The existing m_size value remains the array stride used for modulo offset
   * adjustment. maxSize tracks a statically-known upper bound on the array
   * extent in bytes when available (consumed by the interval-bounded
   * unification in partial collapse).
   *
   * @param sz Array stride in bytes
   * @param maxSize Known upper bound for array extent in bytes, if any
   * @return Reference to this node
   */
  Node &setArraySize(unsigned sz, boost::optional<unsigned> maxSize) {
    assert(!isArray());
    assert(!isForwarding());
    assert(m_size <= sz);

    setArray(true);
    m_size = sz;
    m_arrayMaxSize = maxSize;
    return *this;
  }

  boost::optional<unsigned> getArrayMaxSize() const { return m_arrayMaxSize; }

  /// @brief Check if this node represents an array
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
    assert(g_IsTypeAware || f.getType().isUnknown() || f.getType().isOmniType());
    return m_links.count(Offset::getAdjustedField(*this, f));
  }

  unsigned getNumLinks() const { return m_links.size(); }

  /// @brief Get the total number of collapsed interval cells in this node
  unsigned getNumCollapsedCells() const { return m_collapsedCells.size(); }
  unsigned getNumChunks() const { return getNumCollapsedCells(); }

  /**
   * @brief Get the cell pointed to by a field
   * @param f The field
   * @return The target cell
   */
  const Cell &getLink(Field f) const;

  void setLink(const Field _f, const Cell &c);

  void addLink(Field field, const Cell &c);

  bool hasAccessedType(unsigned offset) const;

  const Set getAccessedType(unsigned o) const {
    Offset offset(*this, o);
    auto it = m_accessedTypes.find(offset.getNumericOffset());
    assert(it != m_accessedTypes.end());
    return it->second;
  }

  bool isVoid() const { return m_accessedTypes.empty(); }
  bool isEmtpyAccessedType() const;

  bool isPartialCollapsed() const { return m_collapsedCells.size() >= 1; }

  /**
   * @brief Check if this node originates from null pointer operations
   *
   * E.g., gep(null, offset) creates a null allocation node.
   *
   * @return true if this is a null allocation
   */
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

/// @brief Check if node is forwarding (inline implementation)
inline bool Node::isForwarding() const { return !m_forward.isNull(); }

/// @brief Get forwarding destination (inline implementation)
inline Cell &Node::getForwardDest() { return m_forward; }

/// @brief Get forwarding destination const (inline implementation)
inline const Cell &Node::getForwardDest() const { return m_forward; }

/**
 * @brief Check if cell has a link at offset (inline implementation)
 * @param offset Field offset to check
 * @return true if link exists
 */
inline bool Cell::hasLink(Field offset) const {
  return m_node && getNode()->hasLink(offset.addOffset(m_offset));
}

/**
 * @brief Get link at offset (inline implementation)
 * @param offset Field offset
 * @return The target cell
 */
inline const Cell &Cell::getLink(Field offset) const {
  assert(m_node);
  // -- call Node::getLink() const
  return static_cast<const Node *>(getNode())->getLink(
      offset.addOffset(m_offset));
}

/**
 * @brief Set link at offset (inline implementation)
 * @param offset Field offset
 * @param c Target cell
 */
inline void Cell::setLink(Field offset, const Cell &c) {
  getNode()->setLink(offset.addOffset(m_offset), c);
}

/**
 * @brief Add link at offset (inline implementation)
 * @param offset Field offset
 * @param c Cell to add/unify
 */
inline void Cell::addLink(Field offset, const Cell &c) {
  getNode()->addLink(offset.addOffset(m_offset), c);
}

/**
 * @brief Add accessed type (inline implementation)
 * @param offset Byte offset
 * @param t Type accessed
 */
inline void Cell::addAccessedType(unsigned offset, llvm::Type *t) {
  getNode()->addAccessedType(m_offset + offset, t);
}

/**
 * @brief Grow cell size (inline implementation)
 * @param o Offset
 * @param t Type at offset
 */
inline void Cell::growSize(unsigned o, llvm::Type *t) {
  assert(!isNull());
  Node::Offset offset(*getNode(), m_offset + o);
  getNode()->growSize(offset, t);
}

/**
 * @brief Get non-forwarding node (inline implementation)
 *
 * Follows forwarding chain to find actual node.
 *
 * @return Pointer to actual node
 */
inline Node *Node::getNode() {
  return isForwarding() ? m_forward.getNode() : this;
}

/**
 * @brief Get non-forwarding node const (inline implementation)
 * @return Pointer to actual node
 */
inline const Node *Node::getNode() const {
  return isForwarding() ? m_forward.getNode() : this;
}

} // namespace seadsa

namespace llvm {
inline raw_ostream &operator<<(raw_ostream &o, const seadsa::Node &n) {
  n.write(o);
  return o;
}
inline raw_ostream &operator<<(raw_ostream &o, const seadsa::Cell &c) {
  c.write(o);
  return o;
}
} // namespace llvm

namespace std {
template <> struct hash<seadsa::Cell> {
  size_t operator()(const seadsa::Cell &c) const {
    size_t seed = 0;
    boost::hash_combine(seed, c.getNode());
    boost::hash_combine(seed, c.getRawOffset());
    const auto end = c.getRawEndOffset();
    boost::hash_combine(seed, static_cast<bool>(end));
    if (end) boost::hash_combine(seed, end.get());
    // boost::hash_combine(seed, c.getType().asTuple());
    return seed;
  }
};
} // namespace std
