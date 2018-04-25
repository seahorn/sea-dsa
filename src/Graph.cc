#include "llvm/IR/Argument.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
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

using namespace llvm;

sea_dsa::Node::Node(Graph &g, const Node &n, bool copyLinks)
    : m_graph(&g), m_unique_scalar(n.m_unique_scalar), m_size(n.m_size) {
  assert(!n.isForwarding());

  // -- copy global id
  m_id = n.m_id;

  // -- copy node type info
  m_nodeType = n.m_nodeType;

  // -- copy types
  joinAccessedTypes(0, n);

  // -- copy allocation sites
  joinAllocSites(n.m_alloca_sites);

  // -- copy links
  if (copyLinks) {
    assert(n.m_graph == m_graph);
    for (auto &kv : n.m_links)
      m_links[kv.first].reset(new Cell(*kv.second));
  }
}
/// adjust offset based on type of the node Collapsed nodes
/// always have offset 0; for array nodes the offset is modulo
/// array size; otherwise offset is not adjusted
sea_dsa::Node::Offset::operator unsigned() const {
  // XXX: m_node can be forward to another node since the constructor
  // of Offset was called so we grab here the non-forwarding node
  Node *n = const_cast<Node *>(m_node.getNode());

  assert(!n->isForwarding());
  if (n->isCollapsed())
    return 0;
  if (n->isArray())
    return m_offset % n->size();
  return m_offset;
}

void sea_dsa::Node::growSize(unsigned v) {
  if (isCollapsed())
    m_size = 1;
  else if (v > m_size) {
    // -- cannot grow size of an array
    if (isArray())
      collapse(__LINE__);
    else
      m_size = v;
  }
}

void sea_dsa::Node::growSize(const Offset &offset, const llvm::Type *t) {
  if (!t)
    return;
  if (t->isVoidTy())
    return;
  if (isCollapsed())
    return;

  assert(m_graph);
  // XXX for some reason getTypeAllocSize() is not marked as preserving const
  auto tSz = m_graph->getDataLayout().getTypeAllocSize(const_cast<Type *>(t));
  growSize(tSz + offset);
}

bool sea_dsa::Node::isEmtpyAccessedType() const {
  return std::all_of(
      std::begin(m_accessedTypes), std::end(m_accessedTypes),
      [](const accessed_types_type::value_type &v) {
        return v.second.isEmpty();
      });
}

bool sea_dsa::Node::hasAccessedType(unsigned o) const {
  if (isCollapsed())
    return false;
  Offset offset(*this, o);
  return m_accessedTypes.count(offset) > 0 &&
         !m_accessedTypes.at(offset).isEmpty();
}

void sea_dsa::Node::addAccessedType(unsigned o, const llvm::Type *t) {
  if (isCollapsed())
    return;
  Offset offset(*this, o);
  growSize(offset, t);
  if (isCollapsed())
    return;

  // -- recursively expand structures
  if (const StructType *sty = dyn_cast<const StructType>(t)) {
    const StructLayout *sl =
        m_graph->getDataLayout().getStructLayout(const_cast<StructType *>(sty));
    unsigned idx = 0;
    for (auto it = sty->element_begin(), end = sty->element_end(); it != end;
         ++it, ++idx) {
      if (getNode()->isCollapsed())
        return;
      unsigned fldOffset = sl->getElementOffset(idx);
      addAccessedType(o + fldOffset, *it);
    }
  }
  // expand array type
  else if (const ArrayType *aty = dyn_cast<const ArrayType>(t)) {
    uint64_t sz =
        m_graph->getDataLayout().getTypeStoreSize(aty->getElementType());
    for (unsigned i = 0, e = aty->getNumElements(); i < e; ++i) {
      if (getNode()->isCollapsed())
        return;
      addAccessedType(o + i * sz, aty->getElementType());
    }
  } else if (const VectorType *vty = dyn_cast<const VectorType>(t)) {
    uint64_t sz = vty->getElementType()->getPrimitiveSizeInBits() / 8;
    for (unsigned i = 0, e = vty->getNumElements(); i < e; ++i) {
      if (getNode()->isCollapsed())
        return;
      addAccessedType(o + i * sz, vty->getElementType());
    }
  }
  // -- add primitive type
  else {
    Set types = m_graph->emptySet();
    if (m_accessedTypes.count(offset) > 0)
      types = m_accessedTypes.at(offset);
    types = m_graph->mkSet(types, t);
    m_accessedTypes.insert(std::make_pair((unsigned)offset, types));
  }
}

void sea_dsa::Node::addAccessedType(const Offset &offset, Set types) {
  if (isCollapsed())
    return;
  for (const llvm::Type *t : types)
    addAccessedType(offset, t);
}

void sea_dsa::Node::joinAccessedTypes(unsigned offset, const Node &n) {
  if (isCollapsed() || n.isCollapsed())
    return;
  for (auto &kv : n.m_accessedTypes) {
    const Offset noff(*this, kv.first + offset);
    addAccessedType(noff, kv.second);
  }
}

/// collapse the current node. Looses all field sensitivity
void sea_dsa::Node::collapse(int tag) {
  if (isCollapsed())
    return;

  LOG("unique_scalar", if (m_unique_scalar) errs()
                           << "KILL due to collapse: " << *m_unique_scalar
                           << "\n";);

  m_unique_scalar = nullptr;
  assert(!isForwarding());
  // if the node is already of smallest size, just mark it
  // collapsed to indicate that it cannot grow or change
  if (size() <= 1) {
    m_size = 1;
    setCollapsed(true);
    return;
  } else {
    LOG("dsa-collapse", errs() << "Collapsing tag: " << tag << "\n";
        write(errs()); errs() << "\n";);

    // create a new node to be the collapsed version of the current one
    // move everything to the new node. This breaks cycles in the links.
    Node &n = m_graph->mkNode();
    n.m_nodeType.join(m_nodeType);
    n.setCollapsed(true);
    n.m_size = 1;
    pointTo(n, Offset(n, 0));
  }
}

void sea_dsa::Node::pointTo(Node &node, const Offset &offset) {
  assert(&node == &offset.node());
  assert(&node != this);
  assert(!isForwarding());

  // -- reset unique scalar at the destination
  if (offset != 0)
    node.setUniqueScalar(nullptr);
  if (m_unique_scalar != node.getUniqueScalar()) {
    LOG("unique_scalar", if (m_unique_scalar && node.getUniqueScalar()) errs()
                             << "KILL due to point-to " << *m_unique_scalar
                             << " and " << *node.getUniqueScalar() << "\n";);

    node.setUniqueScalar(nullptr);
  }

  // unsigned sz = size ();

  // -- create forwarding link
  m_forward.pointTo(node, offset);
  // -- get updated offset based on how forwarding was resolved
  unsigned noffset = m_forward.getRawOffset();
  // -- at this point, current node is being embedded at noffset
  // -- into node

  // // -- grow the size if necessary
  // if (sz + noffset > node.size ()) node.growSize (sz + noffset);

  assert(!node.isForwarding() || node.getNode()->isCollapsed());
  if (!node.getNode()->isCollapsed()) {
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
    if (kv.second->isNodeNull())
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

void sea_dsa::Node::addLink(unsigned o, Cell &c) {
  Offset offset(*this, o);
  if (!hasLink(offset))
    setLink(offset, c);
  else {
    Cell &link = getLink(offset);
    link.unify(c);
  }
}

/// Unify a given node into the current node at a specified offset.
/// Might cause collapse.
void sea_dsa::Node::unifyAt(Node &n, unsigned o) {
  assert(!isForwarding());
  assert(!n.isForwarding());

  // collapse before merging with a collapsed node
  if (!isCollapsed() && n.isCollapsed()) {
    collapse(__LINE__);
    getNode()->unifyAt(*n.getNode(), o);
    return;
  }

  Offset offset(*this, o);

  if (!isCollapsed() && !n.isCollapsed() && n.isArray() && !isArray()) {
    // -- merge into array at offset 0
    if (offset == 0) {
      n.unifyAt(*this, 0);
      return;
    }
    // -- cannot merge array at non-zero offset
    else {
      collapse(__LINE__);
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
      collapse(__LINE__);
      getNode()->unifyAt(*n.getNode(), o);
      return;
    } else {
      Offset minoff(*min, o);
      // -- arrays can only be unified at offset 0
      if (minoff == 0) {
        if (min != this) {
          // unify by merging into smaller array
          n.unifyAt(*this, 0);
          return;
        }
        // otherwise, proceed unifying n into this
      } else {
        // -- cannot unify arrays at non-zero offset
        collapse(__LINE__);
        getNode()->unifyAt(*n.getNode(), o);
        return;
      }
    }
  } else if (isArray() && !n.isArray()) {
    // collapse whenever merging a non-array into an array at non-0 offset
    // and the non-array does not fit into the array
    if (offset != 0 && offset + n.size() > size()) {
      collapse(__LINE__);
      getNode()->unifyAt(*n.getNode(), o);
      return;
    }
  }

  if (&n == this) {
    // -- merging the node into itself at a different offset
    if (offset > 0)
      collapse(__LINE__);
    return;
  }

  assert(!isForwarding());
  assert(!n.isForwarding());
  // -- move everything from n to this node
  n.pointTo(*this, offset);
}

/// pre: this simulated by n
unsigned sea_dsa::Node::mergeUniqueScalar(Node &n) {
  boost::unordered_set<Node *> seen;
  return mergeUniqueScalar(n, seen);
}

template <typename Cache>
unsigned sea_dsa::Node::mergeUniqueScalar(Node &n, Cache &seen) {
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
    unsigned j = kv.first;

    if (hasLink(j)) {
      Node *n1 = getLink(j).getNode();
      res |= n1->mergeUniqueScalar(*n2, seen);
    }
  }

  return res;
}

void sea_dsa::Node::addAllocSite(const Value &v) { m_alloca_sites.insert(&v); }

void sea_dsa::Node::joinAllocSites(const AllocaSet &s) {
  AllocaSet res;
  boost::set_union(m_alloca_sites, s, std::inserter(res, res.end()));
  std::swap(res, m_alloca_sites);
}

// pre: this simulated by n
unsigned sea_dsa::Node::mergeAllocSites(Node &n) {
  boost::unordered_set<Node *> seen;
  return mergeAllocSites(n, seen);
}

template <typename Cache>
unsigned sea_dsa::Node::mergeAllocSites(Node &n, Cache &seen) {
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
    unsigned j = kv.first;

    if (hasLink(j)) {
      Node *n1 = getLink(j).getNode();
      res |= n1->mergeAllocSites(*n2, seen);
    }
  }

  return res;
}

void sea_dsa::Node::writeAccessedTypes(raw_ostream &o) const {
  if (isCollapsed())
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

void sea_dsa::Node::write(raw_ostream &o) const {
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
      o << *a;
    }
    o << "]";
  }
}

void sea_dsa::Cell::dump() const {
  write(errs());
  errs() << "\n";
}

void sea_dsa::Cell::setRead(bool v) { getNode()->setRead(v); }
void sea_dsa::Cell::setModified(bool v) { getNode()->setModified(v); }

bool sea_dsa::Cell::isRead() const { return getNode()->isRead(); }
bool sea_dsa::Cell::isModified() const { return getNode()->isModified(); }

void sea_dsa::Cell::unify(Cell &c) {
  if (isNodeNull()) {
    assert(!c.isNodeNull());
    Node *n = c.getNode();
    pointTo(*n, c.getRawOffset());
  } else if (c.isNodeNull())
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

sea_dsa::Node *sea_dsa::Cell::getNode() const {
  if (isNodeNull())
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

unsigned sea_dsa::Cell::getRawOffset() const {
  // -- resolve forwarding
  getNode();
  // -- return current offset
  return m_offset;
}

unsigned sea_dsa::Cell::getOffset() const {
  // -- adjust the offset based on the kind of node
  if (getNode()->isCollapsed())
    return 0;
  else if (getNode()->isArray())
    return (getRawOffset() % getNode()->size());
  else
    return getRawOffset();
}

void sea_dsa::Cell::pointTo(Node &n, unsigned offset) {
  assert(!n.isForwarding());
  m_node = &n;
  if (n.isCollapsed())
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

unsigned sea_dsa::Node::getRawOffset() const {
  if (!isForwarding())
    return 0;
  m_forward.getNode();
  return m_forward.getRawOffset();
}

sea_dsa::Node &sea_dsa::Graph::mkNode() {
  m_nodes.push_back(std::unique_ptr<Node>(new Node(*this)));
  return *m_nodes.back();
}

sea_dsa::Node &sea_dsa::Graph::cloneNode(const Node &n) {
  m_nodes.push_back(std::unique_ptr<Node>(new Node(*this, n, false)));
  return *m_nodes.back();
}

sea_dsa::Graph::iterator sea_dsa::Graph::begin() {
  return boost::make_indirect_iterator(m_nodes.begin());
}

sea_dsa::Graph::iterator sea_dsa::Graph::end() {
  return boost::make_indirect_iterator(m_nodes.end());
}

sea_dsa::Graph::const_iterator sea_dsa::Graph::begin() const {
  return boost::make_indirect_iterator(m_nodes.begin());
}

sea_dsa::Graph::const_iterator sea_dsa::Graph::end() const {
  return boost::make_indirect_iterator(m_nodes.end());
}

sea_dsa::Graph::scalar_const_iterator sea_dsa::Graph::scalar_begin() const {
  return m_values.begin();
}

sea_dsa::Graph::scalar_const_iterator sea_dsa::Graph::scalar_end() const {
  return m_values.end();
}

sea_dsa::Graph::formal_const_iterator sea_dsa::Graph::formal_begin() const {
  return m_formals.begin();
}

sea_dsa::Graph::formal_const_iterator sea_dsa::Graph::formal_end() const {
  return m_formals.end();
}

sea_dsa::Graph::return_const_iterator sea_dsa::Graph::return_begin() const {
  return m_returns.begin();
}

sea_dsa::Graph::return_const_iterator sea_dsa::Graph::return_end() const {
  return m_returns.end();
}

bool sea_dsa::Graph::IsGlobal::
operator()(const ValueMap::value_type &kv) const {
  return kv.first != nullptr && isa<GlobalValue>(kv.first);
}

sea_dsa::Graph::global_const_iterator sea_dsa::Graph::globals_begin() const {
  return boost::make_filter_iterator(IsGlobal(), m_values.begin(),
                                     m_values.end());
}

sea_dsa::Graph::global_const_iterator sea_dsa::Graph::globals_end() const {
  return boost::make_filter_iterator(IsGlobal(), m_values.end(),
                                     m_values.end());
}

void sea_dsa::Graph::compress() {
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

  // at this point, all cells and all nodes have their links
  // resolved. Every link points directly to the representative of the
  // equivalence class. All forwarding nodes can now be deleted since
  // they have no referrers.

  // -- remove forwarding nodes using remove-erase idiom
  m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                               [](const std::unique_ptr<Node> &n) {
                                 return n->isForwarding();
                               }),
                m_nodes.end());
}

// pre: the graph has been compressed already
void sea_dsa::Graph::remove_dead() {
  LOG("dsa-dead", errs() << "Removing dead nodes ...\n";);

  std::set<const Node *> reachable;

  // --- collect all nodes referenced by scalars
  for (auto &kv : m_values) {
    const Cell *C = kv.second.get();
    if (C->isNodeNull())
      continue;
    if (reachable.insert(C->getNode()).second) {
      LOG("dsa-dead", errs() << "\treachable node " << C->getNode() << "\n";);
    }
  }

  // --- collect all nodes referenced by formal parameters
  for (auto &kv : m_formals) {
    const Cell *C = kv.second.get();
    if (C->isNodeNull())
      continue;
    if (reachable.insert(C->getNode()).second) {
      LOG("dsa-dead", errs() << "\treachable node " << C->getNode() << "\n";);
    }
  }

  // --- collect all nodes referenced by return parameters
  for (auto &kv : m_returns) {
    const Cell *C = kv.second.get();
    if (C->isNodeNull())
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
      if (kv.second->isNodeNull())
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
  m_nodes.erase(std::remove_if(m_nodes.begin(), m_nodes.end(),
                               [reachable](const std::unique_ptr<Node> &n) {
                                 LOG("dsa-dead",
                                     if (reachable.count(&*n) == 0) errs()
                                         << "\tremoving dead " << &*n << "\n";);
                                 return (reachable.count(&*n) == 0);
                               }),
                m_nodes.end());
}

sea_dsa::Cell &sea_dsa::Graph::mkCell(const llvm::Value &u, const Cell &c) {
  auto &v = *u.stripPointerCasts();
  // Pretend that global values are always present
  if (isa<GlobalValue>(&v) && c.isNodeNull()) {
    sea_dsa::Node &n = mkNode();
    n.addAllocSite(v);
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

sea_dsa::Cell &sea_dsa::Graph::mkRetCell(const llvm::Function &fn,
                                         const Cell &c) {
  auto &res = m_returns[&fn];
  if (!res)
    res.reset(new Cell(c));
  return *res;
}

sea_dsa::Cell &sea_dsa::Graph::getRetCell(const llvm::Function &fn) {
  auto it = m_returns.find(&fn);
  assert(it != m_returns.end());
  return *(it->second);
}

const sea_dsa::Cell &
sea_dsa::Graph::getRetCell(const llvm::Function &fn) const {
  auto it = m_returns.find(&fn);
  assert(it != m_returns.end());
  return *(it->second);
}

const sea_dsa::Cell &sea_dsa::Graph::getCell(const llvm::Value &u) {
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

bool sea_dsa::Graph::hasCell(const llvm::Value &u) const {
  auto &v = *u.stripPointerCasts();
  return
      // -- globals are always implicitly present
      isa<GlobalValue>(&v) || m_values.count(&v) > 0 ||
      (isa<Argument>(&v) && m_formals.count(cast<const Argument>(&v)) > 0);
}

void sea_dsa::Cell::write(raw_ostream &o) const {
  getNode();
  o << "<" << m_offset << ",";
  m_node->write(o);
  o << ">";
}

void sea_dsa::Node::dump() const { write(errs()); }

bool sea_dsa::Graph::computeCalleeCallerMapping(
    const DsaCallSite &cs, Graph &calleeG, Graph &callerG,
    SimulationMapper &simMap, const bool reportIfSanityCheckFailed) {
  // XXX: to be removed
  const bool onlyModified = false;

  for (auto &kv : boost::make_iterator_range(calleeG.globals_begin(),
                                             calleeG.globals_end())) {
    Cell &c = *kv.second;
    if (!onlyModified || c.isModified()) {
      Cell &nc = callerG.mkCell(*kv.first, Cell());
      if (!simMap.insert(c, nc)) {
        if (reportIfSanityCheckFailed) {
          errs() << "ERROR: callee is not simulated by caller at "
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
          errs() << "ERROR: callee is not simulated by caller at "
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
            errs() << "ERROR: callee is not simulated by caller at "
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

void sea_dsa::Graph::import(const Graph &g, bool withFormals) {
  Cloner C(*this);
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

void sea_dsa::Graph::write(raw_ostream &o) const {

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

  // TODO: print cells sorted first by node and then by offsets
  // --- print aliasing sets
  o << "=== ALIAS SETS\n";
  for (auto &kv : scalarCells) {
    const Cell *C = kv.first;
    if (kv.second.begin() != kv.second.end()) {
      o << "cell=(" << C->getNode() << "," << C->getRawOffset() << ")\n";
      for (const Value *V : kv.second) {
        o << "\t" << *V << "\n";
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
        o << "\t"
          << "ret(" << F->getName() << ")\n";
      }
    }
  }
}

sea_dsa::Node &sea_dsa::FlatGraph::mkNode() {
  if (m_nodes.empty())
    m_nodes.push_back(std::unique_ptr<Node>(new Node(*this)));

  return *m_nodes.back();
}

// Initialization of static data
uint64_t sea_dsa::Node::m_id_factory = 0;
