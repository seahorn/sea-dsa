#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <set>
#include <string>

#include "sea_dsa/CallSite.hh"
#include "sea_dsa/Cloner.hh"
#include "sea_dsa/Graph.hh"
#include "sea_dsa/Mapper.hh"
#include "sea_dsa/support/Debug.h"

#include "boost/range/algorithm/set_algorithm.hpp"
#include "boost/range/iterator_range.hpp"
#include "boost/unordered_set.hpp"
#include "boost/version.hpp"
#include <boost/pool/pool.hpp>

using namespace llvm;

namespace sea_dsa {
bool IsTypeAware;
}

static llvm::cl::opt<bool, true>
    XTypeAware("sea-dsa-type-aware",
               llvm::cl::desc("Enable SeaDsa type awareness"),
               llvm::cl::location(sea_dsa::IsTypeAware), llvm::cl::init(false));

namespace sea_dsa {

class DsaAllocator {
  boost::pool<> m_pool;
  bool m_use_pool;

public:
  // XXX: by default, memory pool is disabled.
  DsaAllocator(bool use_pool = false)
      : m_pool(256 /* size of chunk */,
               65536 /* number of chunks to grow by */),
        m_use_pool(use_pool){};
  DsaAllocator(const DsaAllocator &o) = delete;
  void *alloc(size_t n) {
    if (m_use_pool) {
      if (n <= m_pool.get_requested_size()) {
        return m_pool.malloc();
      }
    }

    return static_cast<void *>(new char[n]);
  }

  void free(void *block) {
    if (m_use_pool && m_pool.is_from(block)) {
      m_pool.free(block);
    } else {
      delete[] static_cast<char *const>(block);
    }
  }

  DsaAllocatorDeleter getDeleter() { return DsaAllocatorDeleter(*this); }
};
} // namespace sea_dsa

inline void *operator new(size_t n, sea_dsa::DsaAllocator &allocator) {
  return allocator.alloc(n);
}

inline void operator delete(void *p, sea_dsa::DsaAllocator &allocator) {
  allocator.free(p);
}

namespace sea_dsa {
void DsaAllocatorDeleter::operator()(void *block) {
  operator delete(block, *m_allocator);
}

Graph::Graph(const llvm::DataLayout &dl, SetFactory &sf, bool is_flat)
    : m_dl(dl), m_setFactory(sf), m_allocator(new DsaAllocator()),
      m_is_flat(is_flat) {}

Graph::~Graph() = default;
Node::Node(Graph &g)
    : m_graph(&g), m_unique_scalar(nullptr), m_has_once_unique_scalar(false),
      m_size(0), m_id(++m_id_factory) {
  if (!IsTypeAware)
    setTypeCollapsed(true);
}

Node::Node(Graph &g, const Node &n, bool cpLinks, bool cpAllocSites)
    : m_graph(&g), m_unique_scalar(n.m_unique_scalar), m_size(n.m_size) {
  assert(!n.isForwarding());

  // -- fresh id
  m_id = ++m_id_factory;

  // -- copy node type info
  m_nodeType = n.m_nodeType;

  // -- copy types
  joinAccessedTypes(0, n);

  // -- copy allocation sites
  if (cpAllocSites)
    joinAllocSites(n.m_alloca_sites);

  // -- copy links
  if (cpLinks) {
    assert(n.m_graph == m_graph);
    for (auto &kv : n.m_links)
      m_links[kv.first].reset(new Cell(*kv.second));
  }

  if (!IsTypeAware)
    setTypeCollapsed(true);
}
/// adjust offset based on type of the node Collapsed nodes
/// always have offset 0; for array nodes the offset is modulo
/// array size; otherwise offset is not adjusted
unsigned Node::Offset::getNumericOffset() const {
  // XXX: m_node can be forward to another node since the constructor
  // of Offset was called so we grab here the non-forwarding node
  Node *n = const_cast<Node *>(m_node.getNode());
  const unsigned offset = m_offset;

  assert(!n->isForwarding());
  if (n->isOffsetCollapsed())
    return 0;
  if (n->isArray())
    return offset % n->size();
  return offset;
}

void Node::growSize(unsigned v) {
  if (isOffsetCollapsed())
    m_size = 1;
  else if (v > m_size) {
    // -- cannot grow size of an array
    if (isArray())
      collapseOffsets(__LINE__);
    else
      m_size = v;
  }
}

void Node::growSize(const Offset &offset, const llvm::Type *t) {
  if (!t)
    return;
  if (t->isVoidTy())
    return;
  if (isOffsetCollapsed())
    return;

  assert(m_graph);
  // XXX for some reason getTypeAllocSize() is not marked as preserving const
  auto tSz = m_graph->getDataLayout().getTypeAllocSize(const_cast<Type *>(t));
  growSize(tSz + offset.getNumericOffset());
}

bool Node::isEmtpyAccessedType() const {
  return std::all_of(std::begin(m_accessedTypes), std::end(m_accessedTypes),
                     [](const accessed_types_type::value_type &v) {
                       return v.second.isEmpty();
                     });
}

bool Node::hasAccessedType(unsigned o) const {
  if (isOffsetCollapsed())
    return false;
  Offset offset(*this, o);

  auto it = m_accessedTypes.find(offset.getNumericOffset());
  return it != m_accessedTypes.end() && !it->second.isEmpty();
}

void Node::addAccessedType(unsigned off, llvm::Type *type) {
  // Recursion replaced with iteration -- profiles showed this function as a hot
  // spot.
  using WorkList = llvm::SmallVector<std::pair<unsigned, llvm::Type *>, 6>;
  WorkList workList;
  workList.push_back({off, type});

  while (!workList.empty()) {
    std::pair<unsigned, llvm::Type *> offsetType = workList.back();
    workList.pop_back();

    if (isOffsetCollapsed())
      return;

    unsigned o = offsetType.first;
    llvm::Type *t = offsetType.second;

    Offset offset(*this, o);
    growSize(offset, t);
    if (isOffsetCollapsed())
      return;

    // -- recursively expand structures
    if (const StructType *sty = dyn_cast<const StructType>(t)) {
      const StructLayout *sl = m_graph->getDataLayout().getStructLayout(
          const_cast<StructType *>(sty));
      unsigned idx = 0;

      WorkList tmp;
      tmp.reserve(sty->getStructNumElements());
      for (auto it = sty->element_begin(), end = sty->element_end(); it != end;
           ++it, ++idx) {
        unsigned fldOffset = sl->getElementOffset(idx);
        tmp.push_back({o + fldOffset, *it});
      }

      // Append in reverse order to cancel-out reverse caused by the workList
      // stack.
      workList.insert(workList.end(), tmp.rbegin(), tmp.rend());
    }
    // expand array type
    else if (const ArrayType *aty = dyn_cast<const ArrayType>(t)) {
      uint64_t sz =
          m_graph->getDataLayout().getTypeStoreSize(aty->getElementType());
      WorkList tmp;
      const size_t numElements = aty->getNumElements();
      tmp.reserve(numElements);
      llvm::Type *const elementTy = aty->getElementType();

      for (unsigned i = 0; i != numElements; ++i)
        tmp.push_back({o + i * sz, elementTy});

      workList.insert(workList.end(), tmp.rbegin(), tmp.rend());
    } else if (const VectorType *vty = dyn_cast<const VectorType>(t)) {
      uint64_t sz = vty->getElementType()->getPrimitiveSizeInBits() / 8;
      WorkList tmp;
      const size_t numElements(vty->getNumElements());
      tmp.reserve(numElements);
      llvm::Type *const elementTy = vty->getElementType();

      for (size_t i = 0; i != numElements; ++i)
        tmp.push_back({o + i * sz, elementTy});

      workList.insert(workList.end(), tmp.rbegin(), tmp.rend());
    }
    // -- add primitive type
    else {
      Set types = m_graph->emptySet();
      auto it = m_accessedTypes.find(offset.getNumericOffset());
      if (it != m_accessedTypes.end()) {
        types = it->second;
        m_accessedTypes.erase(offset.getNumericOffset());
      }
      types = m_graph->mkSet(types, t);
      m_accessedTypes.insert(std::make_pair(offset.getNumericOffset(), types));
    }
  }
}

void Node::addAccessedType(const Offset &offset, Set types) {
  if (isOffsetCollapsed())
    return;
  for (const llvm::Type *t : types)
    addAccessedType(offset.getNumericOffset(), const_cast<llvm::Type *>(t));
}

void Node::joinAccessedTypes(unsigned offset, const Node &n) {
  if (isOffsetCollapsed() || n.isOffsetCollapsed())
    return;
  for (auto &kv : n.m_accessedTypes) {
    const Offset noff(*this, kv.first + offset);
    addAccessedType(noff, kv.second);
  }
}

/// collapse the current node. Looses all offset-based field sensitivity
void Node::collapseOffsets(int tag) {
  if (isOffsetCollapsed())
    return;

  LOG("unique_scalar",
      if (m_unique_scalar) errs()
          << "KILL due to offset-collapse: " << *m_unique_scalar << "\n";);

  static int cnt = 0;
  ++cnt;
  LOG("dsa-collapse", errs() << "Offset-Collapse #" << cnt << " tag " << tag << "\n");
  // if (cnt == 53) {
  //   errs() << "\n~~~~NOW~~~~\n";
  // }

  m_unique_scalar = nullptr;
  assert(!isForwarding());
  // if the node is already of smallest size, just mark it
  // collapsed to indicate that it cannot grow or change
  if (size() <= 1) {
    m_size = 1;
    setOffsetCollapsed(true);
    return;
  } else {
    LOG("dsa-collapse", errs() << "Offset-Collapsing tag: " << tag << "\n";
        write(errs()); errs() << "\n";);

    // create a new node to be the collapsed version of the current one
    // move everything to the new node. This breaks cycles in the links.
    Node &n = m_graph->mkNode();
    n.m_nodeType.join(m_nodeType);
    n.setOffsetCollapsed(true);
    n.m_size = 1;
    pointTo(n, Offset(n, 0));
  }
}

/// collapse the current node. Looses all type-based field sensitivity
void Node::collapseTypes(int tag) {
  if (isTypeCollapsed())
    return;

  LOG("unique_scalar", if (m_unique_scalar) errs()
                           << "KILL due to type-collapse: " << *m_unique_scalar
                           << "\n";);

  m_unique_scalar = nullptr;
  assert(!isForwarding());

  LOG("dsa-collapse", errs() << "Type-Collapsing tag: " << tag << "\n";
      write(errs()); errs() << "\n";);

  // create a new node to be the collapsed version of the current one
  // move everything to the new node. This breaks cycles in the links.
  Node &n = m_graph->mkNode();
  n.m_nodeType.join(m_nodeType);
  n.setTypeCollapsed(true);
  n.m_size = m_size;
  pointTo(n, Offset(n, 0));
}

void Node::pointTo(Node &node, const Offset &offset) {
  assert(&node == &offset.node());
  assert(&node != this);
  assert(!isForwarding());

  // -- reset unique scalar at the destination
  if (offset.getNumericOffset() != 0)
    node.setUniqueScalar(nullptr);
  if (m_unique_scalar != node.getUniqueScalar()) {
    LOG("unique_scalar", if (m_unique_scalar && node.getUniqueScalar()) errs()
                             << "KILL due to point-to " << *m_unique_scalar
                             << " and " << *node.getUniqueScalar() << "\n";);

    node.setUniqueScalar(nullptr);
  }

  // unsigned sz = size ();

  // -- create forwarding link
  m_forward.pointTo(node, offset.getNumericOffset());
  // -- get updated offset based on how forwarding was resolved
  unsigned noffset = m_forward.getRawOffset();
  // -- at this point, current node is being embedded at noffset
  // -- into node

  // // -- grow the size if necessary
  // if (sz + noffset > node.size ()) node.growSize (sz + noffset);

  assert(!node.isForwarding() || node.getNode()->isOffsetCollapsed() ||
         node.getNode()->isTypeCollapsed());
  if (!node.getNode()->isOffsetCollapsed()) {
    assert(!node.isForwarding());
    // -- merge the types
    node.joinAccessedTypes(noffset, *this);
  }

  // -- merge node annotations
  node.getNode()->m_nodeType.join(m_nodeType);

  // -- merge allocation sites
  node.joinAllocSites(m_alloca_sites);

  // -- move all the links
  for (auto &kv : m_links) {
    if (kv.second->isNull())
      continue;

    m_forward.addLink(kv.first, *kv.second);
  }

  // reset current node
  m_alloca_sites.clear();
  m_size = 0;
  m_links.clear();
  m_accessedTypes.clear();
  m_unique_scalar = nullptr;
  m_nodeType.reset();
}

void Node::addLink(Field f, Cell &c) {
  Offset offset(*this, f.getOffset());
  const Field adjustedField = offset.getAdjustedField(f);

  if (!IsTypeAware)
    assert(f.getType().isUnknown());

  if (!hasLink(adjustedField))
    setLink(adjustedField, c);
  else {
    Cell &link = getLink(adjustedField);
    link.unify(c);
  }
}

/// Unify a given node into the current node at a specified offset.
/// Might cause collapse.
void Node::unifyAt(Node &n, unsigned o) {
  assert(!isForwarding());
  assert(!n.isForwarding());

  // collapse before merging with a collapsed node
  if (!isOffsetCollapsed() && n.isOffsetCollapsed()) {
    collapseOffsets(__LINE__);
    getNode()->unifyAt(*n.getNode(), o);
    return;
  }

  Offset offset(*this, o);

  if (!isOffsetCollapsed() && !n.isOffsetCollapsed() && n.isArray() &&
      !isArray()) {
    // -- merge into array at offset 0
    if (offset.getNumericOffset() == 0) {
      n.unifyAt(*this, 0);
      return;
    }
    // -- cannot merge array at non-zero offset
    else {
      collapseOffsets(__LINE__);
      getNode()->unifyAt(*n.getNode(), o);
      return;
    }
  } else if (isArray() && n.isArray()) {
    // merge larger sized array into 0 offset of the smaller array
    // if the size are compatible
    Node *min = size() <= n.size() ? this : &n;
    Node *max = min == this ? &n : this;

    // sizes are incompatible modulo does not distribute. Hence, we
    // can only shrink an array if the new size is a divisor of all
    // previous non-constant indexes
    if (max->size() % min->size() != 0) {
      collapseOffsets(__LINE__);
      getNode()->unifyAt(*n.getNode(), o);
      return;
    } else {
      Offset minoff(*min, o);
      // -- arrays can only be unified at offset 0
      if (minoff.getNumericOffset() == 0) {
        if (min != this) {
          // unify by merging into smaller array
          n.unifyAt(*this, 0);
          return;
        }
        // otherwise, proceed unifying n into this
      } else {
        // -- cannot unify arrays at non-zero offset
        collapseOffsets(__LINE__);
        getNode()->unifyAt(*n.getNode(), o);
        return;
      }
    }
  } else if (isArray() && !n.isArray()) {
    // collapse whenever merging a non-array into an array at non-0 offset
    // and the non-array does not fit into the array
    if (offset.getNumericOffset() != 0 &&
        offset.getNumericOffset() + n.size() > size()) {
      collapseOffsets(__LINE__);
      getNode()->unifyAt(*n.getNode(), o);
      return;
    }
  }

  if (&n == this) {
    // -- merging the node into itself at a different offset
    if (offset.getNumericOffset() > 0)
      collapseOffsets(__LINE__);
    return;
  }

  assert(!isForwarding());
  assert(!n.isForwarding());
  // -- move everything from n to this node
  n.pointTo(*this, offset);
}

/// pre: this simulated by n
unsigned Node::mergeUniqueScalar(Node &n) {
  boost::unordered_set<Node *> seen;
  return mergeUniqueScalar(n, seen);
}

template <typename Cache>
unsigned Node::mergeUniqueScalar(Node &n, Cache &seen) {
  unsigned res = 0x0;

  auto it = seen.find(&n);
  if (it != seen.end())
    return res;
  seen.insert(&n);

  if (getUniqueScalar() && n.getUniqueScalar()) {
    if (getUniqueScalar() != n.getUniqueScalar()) {
      setUniqueScalar(nullptr);
      n.setUniqueScalar(nullptr);
      res = 0x03;
    }
  } else if (getUniqueScalar()) {
    setUniqueScalar(nullptr);
    res = 0x01;
  } else if (n.getUniqueScalar()) {
    n.setUniqueScalar(nullptr);
    res = 0x02;
  }

  for (auto &kv : n.links()) {
    Node *n2 = kv.second->getNode();
    Field j = kv.first;

    if (hasLink(j)) {
      Node *n1 = getLink(j).getNode();
      res |= n1->mergeUniqueScalar(*n2, seen);
    }
  }

  return res;
}

void Node::addAllocSite(const DsaAllocSite &v) {
  m_alloca_sites.insert(&v.getValue());
}

void Node::joinAllocSites(const AllocaSet &s) {
  using namespace boost;
#if BOOST_VERSION / 100 % 100 < 68
  m_alloca_sites.insert(s.begin(), s.end());
#else
  // At least with boost 1.65 this code does not compile due to an
  // ambiguity problem when make_reverse_iterator is called. There are
  // two found candidates: one in boost and the other one in llvm.
  // With boost 1.68 the ambiguity problem is gone. We don't know with
  // 1.66 and 1.67.
  m_alloca_sites.insert(container::ordered_unique_range_t(), s.begin(),
                        s.end());
#endif
}

// pre: this simulated by n
unsigned Node::mergeAllocSites(Node &n) {
  boost::unordered_set<Node *> seen;
  return mergeAllocSites(n, seen);
}

/// XXX This method needs a comment. @jnavas
template <typename Cache> unsigned Node::mergeAllocSites(Node &n, Cache &seen) {
  unsigned res = 0x0;

  auto it = seen.find(&n);
  if (it != seen.end())
    return res;
  seen.insert(&n);

  auto const &s1 = getAllocSites();
  auto const &s2 = n.getAllocSites();

  if (std::includes(s1.begin(), s1.end(), s2.begin(), s2.end())) {
    if (!std::includes(s2.begin(), s2.end(), s1.begin(), s1.end())) {
      n.joinAllocSites(s1);
      res = 0x2;
    }
  } else if (std::includes(s2.begin(), s2.end(), s1.begin(), s1.end())) {
    joinAllocSites(s2);
    res = 0x1;
  } else {
    joinAllocSites(s2);
    n.joinAllocSites(s1);
    res = 0x3;
  }

  for (auto &kv : n.links()) {
    Node *n2 = kv.second->getNode();
    Field j = kv.first;

    if (hasLink(j)) {
      Node *n1 = getLink(j).getNode();
      res |= n1->mergeAllocSites(*n2, seen);
    }
  }

  return res;
}

void Node::writeAccessedTypes(raw_ostream &o) const {
  if (isOffsetCollapsed())
    o << "collapsed";
  else {
    // Go through all the types, and just print them.
    const accessed_types_type &ts = types();
    bool firstType = true;
    o << "types={";
    if (ts.begin() != ts.end()) {
      for (auto ii = ts.begin(), ee = ts.end(); ii != ee; ++ii) {
        if (!firstType)
          o << ",";
        firstType = false;
        o << ii->first << ":"; // offset
        if (ii->second.begin() != ii->second.end()) {
          bool first = true;
          for (const Type *t : ii->second) {
            if (!first)
              o << "|";
            t->print(o);
            first = false;
          }
        } else
          o << "void";
      }
    } else {
      o << "void";
    }
    o << "}";
  }

  // XXX: this is already printed as a flag
  // if (isArray()) o << " array";
}

void Node::write(raw_ostream &o) const {
  if (isForwarding())
    m_forward.write(o);
  else {
    /// XXX: we print here the address. Therefore, it will change from
    /// one run to another.
    o << "Node " << this << ": ";
    o << "flags=[" << m_nodeType.toStr() << "] ";
    o << "size=" << size() << " ";
    writeAccessedTypes(o);
    o << " links=[";
    bool first = true;
    for (auto &kv : m_links) {
      if (!first)
        o << ",";
      else
        first = false;
      o << kv.first << "->"
        << "(" << kv.second->getOffset() << "," << kv.second->getNode() << ")";
    }
    o << "] ";
    first = true;
    o << " alloca sites=[";
    for (const Value *a : getAllocSites()) {
      if (!first)
        o << ",";
      else
        first = false;
      
      if (const Function *F = dyn_cast<const Function>(a)) {
	o << F->getName() << ":" << *(F->getType());
      } else {
	o << *a;
      }
    }
    o << "]";
  }
}

void Cell::dump() const {
  write(errs());
  errs() << "\n";
}

void Cell::setRead(bool v) { getNode()->setRead(v); }
void Cell::setModified(bool v) { getNode()->setModified(v); }

bool Cell::isRead() const { return getNode()->isRead(); }
bool Cell::isModified() const { return getNode()->isModified(); }

void Cell::unify(Cell &c) {
  if (isNull()) {
    assert(!c.isNull());
    Node *n = c.getNode();
    pointTo(*n, c.getRawOffset());
  } else if (c.isNull())
    c.unify(*this);
  else {
    Node &n1 = *getNode();
    unsigned o1 = getRawOffset();

    Node &n2 = *c.getNode();
    unsigned o2 = c.getRawOffset();

    if (o1 < o2)
      n2.unifyAt(n1, o2 - o1);
    else if (o2 < o1)
      n1.unifyAt(n2, o1 - o2);
    else /* o1 == o2 */
      // TODO: other ways to break ties
      n1.unify(n2);
  }
}

Node *Cell::getNode() const {
  if (isNull())
    return nullptr;

  Node *n = m_node->getNode();
  assert((n == m_node && !m_node->isForwarding()) || m_node->isForwarding());
  if (n != m_node) {
    assert(m_node->isForwarding());
    m_offset += m_node->getRawOffset();
    m_node = n;
  }

  return m_node;
}

unsigned Cell::getRawOffset() const {
  // -- resolve forwarding
  getNode();
  // -- return current offset
  return m_offset;
}

unsigned Cell::getOffset() const {
  // -- adjust the offset based on the kind of node
  if (isNull() || getNode()->isOffsetCollapsed())
    return 0;
  else if (getNode()->isArray())
    return (getRawOffset() % getNode()->size());
  else
    return getRawOffset();
}

void Cell::pointTo(Node &n, unsigned offset) {
  assert(!n.isForwarding());
  // n.viewGraph();
  m_node = &n;
  // errs() << "dsads\n";
  if (n.isOffsetCollapsed())
    m_offset = 0;
  else if (n.isArray()) {
    assert(n.size() > 0);
    m_offset = offset % n.size();
  } else {
    /// grow size as needed. allow offset to go one byte past size
    if (offset < n.size())
      n.growSize(offset);
    m_offset = offset;
  }
}

unsigned Node::getRawOffset() const {
  if (!isForwarding())
    return 0;
  m_forward.getNode();
  return m_forward.getRawOffset();
}

Node &Graph::mkNode() {
  m_nodes.emplace_back(new (*m_allocator) Node(*this),
                       m_allocator->getDeleter());
  return *m_nodes.back();
}

Node &Graph::cloneNode(const Node &n, bool cpAllocSites) {
  m_nodes.emplace_back(new (*m_allocator) Node(*this, n, false, cpAllocSites),
                       m_allocator->getDeleter());
  return *m_nodes.back();
}

Graph::iterator Graph::begin() {
  return boost::make_indirect_iterator(m_nodes.begin());
}

Graph::iterator Graph::end() {
  return boost::make_indirect_iterator(m_nodes.end());
}

Graph::const_iterator Graph::begin() const {
  return boost::make_indirect_iterator(m_nodes.begin());
}

Graph::const_iterator Graph::end() const {
  return boost::make_indirect_iterator(m_nodes.end());
}

Graph::scalar_const_iterator Graph::scalar_begin() const {
  return m_values.begin();
}

Graph::scalar_const_iterator Graph::scalar_end() const {
  return m_values.end();
}

Graph::formal_const_iterator Graph::formal_begin() const {
  return m_formals.begin();
}

Graph::formal_const_iterator Graph::formal_end() const {
  return m_formals.end();
}

Graph::return_const_iterator Graph::return_begin() const {
  return m_returns.begin();
}

Graph::return_const_iterator Graph::return_end() const {
  return m_returns.end();
}

bool Graph::IsGlobal::operator()(const ValueMap::value_type &kv) const {
  return kv.first != nullptr && isa<GlobalValue>(kv.first);
}

Graph::global_const_iterator Graph::globals_begin() const {
  return boost::make_filter_iterator(IsGlobal(), m_values.begin(),
                                     m_values.end());
}

Graph::global_const_iterator Graph::globals_end() const {
  return boost::make_filter_iterator(IsGlobal(), m_values.end(),
                                     m_values.end());
}

void Graph::compress() {
  // -- resolve all forwarding
  for (auto &n : m_nodes) {
    // resolve the node
    n->getNode();
    // if not forwarding, resolve forwarding on all links
    if (!n->isForwarding()) {
      n->compress();
      for (auto &kv : n->links())
        kv.second->getNode();
    }
  }

  for (auto &kv : m_values)
    kv.second->getNode();
  for (auto &kv : m_formals)
    kv.second->getNode();
  for (auto &kv : m_returns)
    kv.second->getNode();
  for (auto &cs: m_callSites)
    cs->getCell().getNode();
  
  // at this point, all cells and all nodes have their links
  // resolved. Every link points directly to the representative of the
  // equivalence class. All forwarding nodes can now be deleted since
  // they have no referrers.

  // -- remove forwarding nodes using remove-erase idiom
  m_nodes.erase(
      std::remove_if(m_nodes.begin(), m_nodes.end(),
                     [](const std::unique_ptr<Node, DsaAllocatorDeleter> &n) {
                       return n->isForwarding();
                     }),
      m_nodes.end());
}

// pre: the graph has been compressed already
void Graph::remove_dead() {
  LOG("dsa-dead", errs() << "Removing dead nodes ...\n";);

  std::set<const Node *> reachable;

  // --- collect all nodes referenced by scalars
  for (auto &kv : m_values) {
    const Cell *C = kv.second.get();
    if (C->isNull())
      continue;
    if (reachable.insert(C->getNode()).second) {
      LOG("dsa-dead", errs() << "\treachable node " << C->getNode() << "\n";);
    }
  }

  // --- collect all nodes referenced by formal parameters
  for (auto &kv : m_formals) {
    const Cell *C = kv.second.get();
    if (C->isNull())
      continue;
    if (reachable.insert(C->getNode()).second) {
      LOG("dsa-dead", errs() << "\treachable node " << C->getNode() << "\n";);
    }
  }

  // --- collect all nodes referenced by return parameters
  for (auto &kv : m_returns) {
    const Cell *C = kv.second.get();
    if (C->isNull())
      continue;
    if (reachable.insert(C->getNode()).second) {
      LOG("dsa-dead", errs() << "\treachable node " << C->getNode() << "\n";);
    }
  }

  // --- compute all reachable nodes from referenced nodes
  std::vector<const Node *> worklist(reachable.begin(), reachable.end());
  while (!worklist.empty()) {
    auto n = worklist.back();
    worklist.pop_back();
    for (auto &kv : n->links()) {
      if (kv.second->isNull())
        continue;
      auto s = kv.second->getNode();
      if (reachable.insert(s).second) {
        worklist.push_back(s);
        LOG("dsa-dead", errs()
                            << "\t" << s << " reachable from " << n << "\n";);
      }
    }
  }

  // -- remove unreachable nodes using remove-erase idiom
  m_nodes.erase(
      std::remove_if(
          m_nodes.begin(), m_nodes.end(),
          [reachable](const std::unique_ptr<Node, DsaAllocatorDeleter> &n) {
            LOG("dsa-dead", if (reachable.count(&*n) == 0) errs()
                                << "\tremoving dead " << &*n << "\n";);
            return (reachable.count(&*n) == 0);
          }),
      m_nodes.end());
}

Cell &Graph::mkCell(const llvm::Value &u, const Cell &c) {
  auto &v = *u.stripPointerCasts();
  // Pretend that global values are always present
  if (isa<GlobalValue>(&v) && c.isNull()) {
    Node &n = mkNode();
    DsaAllocSite *site = mkAllocSite(v);
    assert(site);
    n.addAllocSite(*site);
    return mkCell(v, Cell(n, 0));
  }

  auto &res =
      isa<Argument>(v) ? m_formals[cast<const Argument>(&v)] : m_values[&v];
  if (!res) {
    res.reset(new Cell(c));

    if (res->getRawOffset() == 0 && res->getNode()) {
      if (!(res->getNode()->hasOnceUniqueScalar()))
        res->getNode()->setUniqueScalar(&v);
      else {
        LOG("unique_scalar", errs() << "KILL due to mkCell: ";
            if (res->getNode()->getUniqueScalar()) errs()
            << "OLD: " << *res->getNode()->getUniqueScalar();
            errs() << " NEW: " << v << "\n";);
        res->getNode()->setUniqueScalar(nullptr);
      }
    }

    if (res->getRawOffset() != 0 && res->getNode() &&
        res->getNode()->hasOnceUniqueScalar()) {
      const auto *UniqueScalar = res->getNode()->getUniqueScalar();
      (void)UniqueScalar;
      LOG("unique_scalar", if (UniqueScalar) errs() << "KILL due to mkCell: "
                                                    << "OLD: " << *UniqueScalar
                                                    << " NEW: " << v << "\n");
      res->getNode()->setUniqueScalar(nullptr);
    }
  }

  return *res;
}

// Remove any _direct_ link from n to another cell whose node satisfies p.
void Graph::removeLinks(Node *n, std::function<bool(const Node *)> p) {
  assert(!n->isForwarding());

  std::set<unsigned> removed_offsets;
  auto &links = n->getLinks();
  links.erase(std::remove_if(
                  links.begin(), links.end(),
                  [p, &removed_offsets](const std::pair<Field, CellRef> &link) {
                    const CellRef &next = link.second;
                    if (p(&*(next->getNode()))) {
                      removed_offsets.insert(link.first.getOffset());
                      return true;
                    } else {
                      return false;
                    }
                  }),
              links.end());

  // Remove accessed types, not link types.
  auto &types = n->types();
  for (auto it = types.begin(), et = types.end(); it != et;) {
    if (removed_offsets.count(it->first)) {
      auto cur_it = it;
      ++it;
      types.erase(cur_it);
    } else {
      ++it;
    }
  }
}

// Remove all nodes that satisfy p and links to nodes that satisfy p.
void Graph::removeNodes(std::function<bool(const Node *)> p) {
  if (m_nodes.empty())
    return;

  // remove entry if it references to a node that satisfies p
  for (auto it = m_values.begin(), et = m_values.end(); it != et;) {
    Cell *C = it->second.get();
    assert(!C->isNull());
    if (p(C->getNode())) {
      auto cur_it = it;
      ++it;
      m_values.erase(cur_it);
    } else {
      ++it;
    }
  }

  // remove entry if it references to a node that satisfies p
  for (auto it = m_formals.begin(), et = m_formals.end(); it != et;) {
    Cell *C = it->second.get();
    assert(!C->isNull());
    if (p(C->getNode())) {
      auto cur_it = it;
      ++it;
      m_formals.erase(cur_it);
    } else {
      ++it;
    }
  }

  // remove entry if it references to a node that satisfies p
  for (auto it = m_returns.begin(), et = m_returns.end(); it != et;) {
    Cell *C = it->second.get();
    assert(!C->isNull());
    if (p(C->getNode())) {
      auto cur_it = it;
      ++it;
      m_returns.erase(cur_it);
    } else {
      ++it;
    }
  }

  // -- remove references to nodes that satisfy p
  for (auto &n : m_nodes) {
    if (!n->isForwarding()) {
      removeLinks(&*n, p);
    }
  }

  // -- remove nodes that satisfy p

  // (**) At this point we should have either unreachable nodes
  // satisfying p or unreachable nodes forwarding to nodes satisfying
  // p.

  m_nodes.erase(
      std::remove_if(m_nodes.begin(), m_nodes.end(),
                     [p](const std::unique_ptr<Node, DsaAllocatorDeleter> &n) {
                       // resolve node so that we also remove a node
                       // if it's forwarding to a node that satifies
                       // p.
                       return (p(n->getNode()));
                     }),
      m_nodes.end());

  // FIXME: the assumption (**) probably is not true because if we
  // don't call remove_dead() we crash later because some dangling
  // node.
  remove_dead();
}

Cell &Graph::mkRetCell(const llvm::Function &fn, const Cell &c) {
  auto &res = m_returns[&fn];
  if (!res)
    res.reset(new Cell(c));
  return *res;
}

Cell &Graph::getRetCell(const llvm::Function &fn) {
  auto it = m_returns.find(&fn);
  assert(it != m_returns.end());
  return *(it->second);
}

const Cell &Graph::getRetCell(const llvm::Function &fn) const {
  auto it = m_returns.find(&fn);
  assert(it != m_returns.end());
  return *(it->second);
}

const Cell &Graph::getCell(const llvm::Value &u) {
  const llvm::Value &v = *(u.stripPointerCasts());
  // -- try m_formals first
  if (const llvm::Argument *arg = dyn_cast<const Argument>(&v)) {
    auto it = m_formals.find(arg);
    assert(it != m_formals.end());
    return *(it->second);
  } else if (isa<GlobalValue>(&v))
    return mkCell(v, Cell());
  else {
    auto it = m_values.find(&v);
    assert(it != m_values.end());
    return *(it->second);
  }
}

bool Graph::hasCell(const llvm::Value &u) const {
  auto &v = *u.stripPointerCasts();
  return
      // -- globals are always implicitly present
      isa<GlobalValue>(&v) || m_values.count(&v) > 0 ||
      (isa<Argument>(&v) && m_formals.count(cast<const Argument>(&v)) > 0);
}

// static bool isIntToPtrConstant(const llvm::Value &v) {
//   if (auto *inttoptr = dyn_cast<ConstantExpr>(&v)) {
//     if (inttoptr->getOpcode() == Instruction::IntToPtr) {
//       return true;
//     }
//   }
//   return false;
// }

DsaAllocSite *sea_dsa::Graph::mkAllocSite(const llvm::Value &v) {
  auto res = getAllocSite(v);
  if (res.hasValue())
    return res.getValue();

  m_allocSites.emplace_back(new DsaAllocSite(*this, v));
  DsaAllocSite *as = m_allocSites.back().get();
  m_valueToAllocSite.insert(std::make_pair(&v, as));
  return as;
}

DsaCallSite *Graph::mkCallSite(const llvm::Instruction &I, Cell c) {
  auto it = m_instructionToCallSite.find(&I);
  if (it != m_instructionToCallSite.end()) {
    return it->second;
  }

  m_callSites.emplace_back(new DsaCallSite(I, c));
  DsaCallSite *cs = m_callSites.back().get();
  m_instructionToCallSite.insert(std::make_pair(&I, cs));
  return cs;
}

llvm::iterator_range<typename Graph::callsite_iterator> Graph::callsites() {
  return llvm::make_range(m_callSites.begin(), m_callSites.end());
}

llvm::iterator_range<typename Graph::callsite_const_iterator> Graph::callsites() const {
  return llvm::make_range(m_callSites.begin(), m_callSites.end());
}

void Graph::clearCallSites() {
  m_instructionToCallSite.clear();
  m_callSites.clear();
}

void Cell::write(raw_ostream &o) const {
  getNode();
  o << "<" << m_offset << ", ";
  if (m_node)
    m_node->write(o);
  else
    o << "null";
  o << ">";
}

void Node::dump() const {
  write(errs());
  errs() << "\n";
}

void Node::viewGraph() { getGraph()->viewGraph(); }

bool Graph::computeCalleeCallerMapping(const DsaCallSite &cs, Graph &calleeG,
                                       Graph &callerG, SimulationMapper &simMap,
                                       const bool reportIfSanityCheckFailed) {
  // XXX: to be removed
  const bool onlyModified = false;

  for (auto &kv : boost::make_iterator_range(calleeG.globals_begin(),
                                             calleeG.globals_end())) {
    Cell &c = *kv.second;
    if (!onlyModified || c.isModified()) {
      Cell &nc = callerG.mkCell(*kv.first, Cell());
      if (!simMap.insert(c, nc)) {
        if (reportIfSanityCheckFailed) {
          errs() << "ERROR 1: callee is not simulated by caller at "
                 << *cs.getInstruction() << "\n"
                 << "\tGlobal: " << *kv.first << "\n"
                 << "\tCallee cell=" << c << "\n"
                 << "\tCaller cell=" << nc << "\n";
        }
        return false;
      }
    }
  }

  const Function &callee = *cs.getCallee();
  if (calleeG.hasRetCell(callee) && callerG.hasCell(*cs.getInstruction())) {
    const Cell &c = calleeG.getRetCell(callee);
    if (!onlyModified || c.isModified()) {
      Cell &nc = callerG.mkCell(*cs.getInstruction(), Cell());
      if (!simMap.insert(c, nc)) {
        if (reportIfSanityCheckFailed) {
          errs() << "ERROR 2: callee is not simulated by caller at "
                 << *cs.getInstruction() << "\n"
                 << "\rReturn value of " << callee.getName() << "\n"
                 << "\rCallee cell=" << c << "\n"
                 << "\rCaller cell=" << nc << "\n";
        }
        return false;
      }
    }
  }

  DsaCallSite::const_actual_iterator AI = cs.actual_begin(),
                                     AE = cs.actual_end();
  for (DsaCallSite::const_formal_iterator FI = cs.formal_begin(),
                                          FE = cs.formal_end();
       FI != FE && AI != AE; ++FI, ++AI) {
    const Value *fml = &*FI;
    const Value *arg = (*AI).get();

    if (calleeG.hasCell(*fml) && callerG.hasCell(*arg)) {
      Cell &c = calleeG.mkCell(*fml, Cell());
      if (!onlyModified || c.isModified()) {
        Cell &nc = callerG.mkCell(*arg, Cell());
        if (!simMap.insert(c, nc)) {
          if (reportIfSanityCheckFailed) {
            errs() << "ERROR 3: callee is not simulated by caller at "
                   << *cs.getInstruction() << "\n"
                   << "\tFormal param " << *fml << "\n"
                   << "\tActual param " << *arg << "\n"
                   << "\tCallee cell=" << c << "\n"
                   << "\tCaller cell=" << nc << "\n";
          }
          return false;
        }
      }
    }
  }
  return true;
}

void Graph::import(const Graph &g, bool withFormals) {
  Cloner C(*this, CloningContext::mkNoContext(), Cloner::Options::Basic);
  for (auto &kv : g.m_values) {
    // -- clone node
    Node &n = C.clone(*kv.second->getNode());

    // -- re-create the cell
    Cell c(n, kv.second->getRawOffset());

    // -- insert value
    Cell &nc = mkCell(*kv.first, Cell());

    // -- unify the old and new cells
    nc.unify(c);
  }

  if (withFormals) {
    for (auto &kv : g.m_formals) {
      Node &n = C.clone(*kv.second->getNode());
      Cell c(n, kv.second->getRawOffset());
      Cell &nc = mkCell(*kv.first, Cell());
      nc.unify(c);
    }
    for (auto &kv : g.m_returns) {
      Node &n = C.clone(*kv.second->getNode());
      Cell c(n, kv.second->getRawOffset());
      Cell &nc = mkRetCell(*kv.first, Cell());
      nc.unify(c);
    }
  }

  // possibly created many indirect links, compress
  compress();
}

void Graph::write(raw_ostream &o) const {

  typedef std::set<const llvm::Value *> ValSet;
  typedef std::set<const llvm::Argument *> ArgSet;
  typedef std::set<const llvm::Function *> FuncSet;

  typedef DenseMap<const Cell *, ValSet> CellValMap;
  typedef DenseMap<const Cell *, ArgSet> CellArgMap;
  typedef DenseMap<const Cell *, FuncSet> CellRetMap;

  // --- collect all nodes and cells referenced by scalars
  CellValMap scalarCells;
  for (auto &kv : m_values) {
    const Cell *C = kv.second.get();
    auto it = scalarCells.find(C);
    if (it == scalarCells.end()) {
      ValSet S;
      S.insert(kv.first);
      scalarCells.insert(std::make_pair(C, S));
    } else {
      it->second.insert(kv.first);
    }
  }

  // --- collect all nodes and cells referenced by function formal
  //     parameters
  CellArgMap argCells;
  for (auto &kv : m_formals) {
    const Cell *C = kv.second.get();
    auto it = argCells.find(C);
    if (it == argCells.end()) {
      ArgSet S;
      S.insert(kv.first);
      argCells.insert(std::make_pair(C, S));
    } else {
      it->second.insert(kv.first);
    }
  }

  // --- collect all nodes and cells referenced by return parameters
  CellRetMap retCells;
  for (auto &kv : m_returns) {
    const Cell *C = kv.second.get();
    auto it = retCells.find(C);
    if (it == retCells.end()) {
      FuncSet S;
      S.insert(kv.first);
      retCells.insert(std::make_pair(C, S));
    } else {
      it->second.insert(kv.first);
    }
  }

  // --- print all nodes
  o << "=== NODES\n";
  for (auto &N : m_nodes) {
    N->write(o);
    o << "\n";
  }

  // --- print aliasing sets
  // TODO: print LLVM registers in equivalence classes (grouped by cells)
  o << "=== TOP-LEVEL CELLS\n";
  for (auto &kv : scalarCells) {
    const Cell *C = kv.first;
    if (kv.second.begin() != kv.second.end()) {
      o << "cell=(" << C->getNode() << "," << C->getRawOffset() << ")\n";
      for (const Value *V : kv.second) {
	if (const Function* F = dyn_cast<const Function>(V)) {
	  o << "\t" << F->getName() << ":" << *(F->getType()) << "\n";
	} else {
	  o << "\t" << *V << "\n";
	}
      }
    }
  }
  for (auto &kv : argCells) {
    const Cell *C = kv.first;
    if (kv.second.begin() != kv.second.end()) {
      o << "cell=(" << C->getNode() << "," << C->getRawOffset() << ")\n";
      for (const Argument *A : kv.second) {
        o << "\t" << *A << "\n";
      }
    }
  }
  for (auto &kv : retCells) {
    const Cell *C = kv.first;
    if (kv.second.begin() != kv.second.end()) {
      o << "cell=(" << C->getNode() << "," << C->getRawOffset() << ")\n";
      for (const Function *F : kv.second) {
        o << "\tret(" << F->getName() << ")\n";
      }
    }
  }

  // if (!m_callSites.empty()) {
  //   o << "=== INDIRECT CALLSITES\n";
  //   for (unsigned i = 0, e = m_callSites.size(); i < e; ++i) {
  //     m_callSites[i]->write(o);
  //     o << "\n";
  //   }
  // }
  
}

size_t Graph::numCollapsed() const {
  return std::count_if(
      m_nodes.begin(), m_nodes.end(),
      [](const NodeVectorElemTy &N) { return N->isOffsetCollapsed(); });
}

void Graph::dump() const { write(errs()); }

void Graph::viewGraph() { ShowDsaGraph(*this); }

Node &FlatGraph::mkNode() {
  if (m_nodes.empty())
    m_nodes.emplace_back(new (*m_allocator) Node(*this),
                         m_allocator->getDeleter());

  return *m_nodes.back();
}

// Initialization of static data
uint64_t Node::m_id_factory = 0;
} // namespace sea_dsa
