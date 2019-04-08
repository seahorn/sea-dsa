#include "llvm/Support/raw_ostream.h"
#include "sea_dsa/Mapper.hh"

using namespace sea_dsa;

void FunctionalMapper::insert(const Cell &src, const Cell &dst) {
  assert (!src.isNull());
  assert (!dst.isNull());

  // -- already mapped
  Cell res = get(src);
  if (!res.isNull()) return;

  res = get(*src.getNode());
  if (!res.isNull())
    assert (res.getNode() == dst.getNode() && "No functional node mapping");

  assert (src.getRawOffset() <= dst.getRawOffset() && "Not supported");

  // -- offset of the source node in the destination
  unsigned srcNodeOffset = dst.getRawOffset() - src.getRawOffset();
  m_nodes.emplace(src.getNode(), Cell(dst.getNode(), srcNodeOffset));
  m_cells.emplace(src, dst);

  Node::Offset srcOffset(*src.getNode(), src.getRawOffset());

  // -- process all the links
  // XXX: Don't think this properly handles aligning array nodes of different
  //     sizes
  for (auto &kv : src.getNode()->links()) {
    if (kv.first.getOffset() < srcOffset.getNumericOffset())
      continue;
    if (dst.getNode()->hasLink(kv.first.addOffset(srcNodeOffset)))
      insert(*kv.second,
             dst.getNode()->getLink(kv.first.addOffset(srcNodeOffset)));
  }
}

bool SimulationMapper::insert(const Cell &c1, Cell &c2) {
  if (c1.isNull() != c2.isNull()) {
    m_sim.clear();
    return false;
  }

  if (c1.isNull())
    return true;

  if (c2.getNode()->isOffsetCollapsed())
    return insert(*c1.getNode(), *c2.getNode(),
                  Field(0, FIELD_TYPE_NOT_IMPLEMENTED));

  // XXX: adjust the offsets
  Node::Offset o1(*c1.getNode(), c1.getRawOffset());
  Node::Offset o2(*c2.getNode(), c2.getRawOffset());

  if (o2.getNumericOffset() < o1.getNumericOffset()) {
    if (!c1.getNode()->isArray() && c2.getNode()->isArray()) {
      // do nothing: covered below
    } else {
      m_sim.clear();
      return false;
    }
  }

  Field f(o2.getNumericOffset() - o1.getNumericOffset(),
          FIELD_TYPE_NOT_IMPLEMENTED);
  return insert(*c1.getNode(), *c2.getNode(), f);
}

// Return true iff n1 (at offset 0) is simulated by n2 at offset o
bool SimulationMapper::insert(const Node &n1, Node &n2, Field o)
{
  // XXX: adjust the offset
  Node::Offset offset(n2, o.getOffset());

  auto &map = m_sim[&n1];
  if (map.count (&n2) > 0)
  { // n1 is simulated by n2 at *adjusted* offset 
    if (map.at (&n2).getOffset() == offset.getNumericOffset()) return true;
    m_sim.clear (); return false;
  }
  
  // -- not array can be simulated by array of larger size at offset 0
  // XXX probably sufficient if n1 can be completely embedded into n2,
  // XXX not necessarily at offset 0
  if (!n1.isArray () && n2.isArray ())
  {
    // XXX: n1 is an unfolding of n2, we can map it.
    if (n1.size() >= n2.size() && (n1.size() % n2.size() == 0) &&
        offset.getNumericOffset() == 0) {
      ; // Do nothing.
    } else if (offset.getNumericOffset() > 0 && n1.size () + o.getOffset() > n2.size ())
      { m_sim.clear (); return false; }
  }

  // XXX: a collapsed node can simulate an array node
  if (n1.isArray () && (!n2.isArray () && !n2.isOffsetCollapsed()))
  { m_sim.clear (); return false; }

  if (n1.isArray () && offset.getNumericOffset() != 0)
  { m_sim.clear (); return false; } 
    
  // XXX: a collapsed node can simulate an array node
  if (n1.isArray () && !n2.isOffsetCollapsed() && n1.size () != n2.size ()) {
    // XXX: two arrays are properly aligned, and n2 is smaller than n1,
    //      and n1 is an unfolding of n2, we can map it.
    if (n2.isArray() && n2.size() < n1.size() &&
        (n1.size() % n2.size() == 0) && offset.getNumericOffset() == 0) {
      ; // Do nothing.
    } else {
      m_sim.clear();
      return false;
    }
  }
  
  if (n1.isOffsetCollapsed() && !n2.isOffsetCollapsed())
  { m_sim.clear (); return false; }
      
  // At this point, n1 (at offset 0) is simulated by n2 at adjusted
  // offset. Thus, we add n2 into the map.
  map.emplace(&n2, offset.getAdjustedField(o));

  // Check children recursively
  for (auto &kv : n1.links ())
  {
    const unsigned j = n2.isOffsetCollapsed() ? 0 : kv.first.getOffset() +
                                         offset.getNumericOffset();
    const FieldType ty = kv.first.getType();
    const Field adjusted(j, ty);

    if (!n2.hasLink(adjusted))
    { m_sim.clear (); return false; }
    
    Node *n3 = kv.second->getNode ();
    // adjusted offset
    unsigned off1 = kv.second->getOffset ();

    auto &link = n2.getLink(adjusted);
    Node *n4 = link.getNode ();
    // adjusted offset
    unsigned off2 = link.getOffset ();

    // Account for arrays and wrap around -- array simulates its unfolding.
    // Note: simulation is not symmetric -- doesn't work the other way round.
    if (n4->isArray()) {
      Node::Offset _offset(*n4, off1);
      off1 = _offset.getNumericOffset();
    }

    // Offsets must be adjusted
    if (off2 < off1 && !n4->isOffsetCollapsed())
    { m_sim.clear (); return false; }

    // Offsets must be adjusted     
    if (!insert (*n3, *n4, Field(off2 - off1, ty)))
    { m_sim.clear (); return false; }
  }
  
  return true;
}

void SimulationMapper::write(llvm::raw_ostream&o) const 
{
  o << "BEGIN simulation mapper\n";
  for (auto &kv: m_sim)
    for (auto &c: kv.second) 
      o << "(" << *(kv.first) << ", " << *(c.first) << ", " << c.second << ")\n";
  o << "END  simulation mapper\n";
}

bool SimulationMapper::isInjective (bool onlyModified)  const 
{
  boost::container::flat_set<Cell> inv_sim;
  for (auto &kv: m_sim) 
    for (auto &c: kv.second) 
    {
      auto res = inv_sim.insert(Cell(c.first, c.second.getOffset()));
      if (!onlyModified || c.first->isModified()) 
        if (!res.second) return false;
    }
  return true;
}

bool SimulationMapper::isFunction () const 
{
  for (auto &kv: m_sim)
    if (kv.second.size () > 1) return false;
  return true;
}
