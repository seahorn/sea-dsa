#include "llvm/Support/raw_ostream.h"
#include "sea_dsa/Mapper.hh"

using namespace sea_dsa;

void FunctionalMapper::insert (const Cell &src, const Cell &dst)
{
  assert (!src.isNull());
  assert (!dst.isNull());

  // -- already mapped
  Cell res = get (src);
  if (!res.isNull()) return;

  res = get (*src.getNode ());
  if (!res.isNull())
    assert (res.getNode () == dst.getNode () && "No functional node mapping");
  
  assert (src.getRawOffset () <= dst.getRawOffset () && "Not supported");

  // -- offset of the source node in the destination
  unsigned srcNodeOffset = dst.getRawOffset () - src.getRawOffset ();

  // FIXME: Types
  m_nodes.insert (std::make_pair (src.getNode (), Cell (dst.getNode (), srcNodeOffset, dst.getType())));
  m_cells.insert (std::make_pair (src, dst));
  
  Node::Offset srcOffset (*src.getNode (), src.getRawOffset ());
  
  // -- process all the links
  // XXX Don't think this properly handles aligning array nodes of different sizes
  for (auto &kv : src.getNode ()->links ())
  {
    if (kv.first.getOffset() < srcOffset.getNumericOffset()) continue;
    if (dst.getNode()->hasLink(kv.first.addOffset(srcNodeOffset)))
      insert(*kv.second, dst.getNode()->getLink(kv.first.addOffset(srcNodeOffset)));
  }
}

bool SimulationMapper::insert (const Cell &c1, Cell &c2)
{
  if (c1.isNull() != c2.isNull())
  { m_sim.clear (); return false; }

  if (c1.isNull()) return true;

  if (c2.getNode()->isCollapsed())
    return insert (*c1.getNode (), *c2.getNode (), 0);

  // XXX: adjust the offsets
  Node::Offset o1 (*c1.getNode(), c1.getRawOffset());
  Node::Offset o2 (*c2.getNode(), c2.getRawOffset());
    
  if (o2.getNumericOffset() < o1.getNumericOffset())
  { m_sim.clear (); return false; }
  
  return insert (*c1.getNode (), *c2.getNode (), o2.getNumericOffset() -
                                                 o1.getNumericOffset());
}

// Return true iff n1 (at offset 0) is simulated by n2 at offset o
bool SimulationMapper::insert (const Node &n1, Node &n2, unsigned o)
{
  // XXX: adjust the offset
  Node::Offset offset (n2, o);

  auto &map = m_sim[&n1];
  if (map.count (&n2) > 0)
  { // n1 is simulated by n2 at *adjusted* offset 
    if (map.at (&n2) == offset.getNumericOffset()) return true;
    m_sim.clear (); return false;
  }
  
  // -- not array can be simulated by array of larger size at offset 0
  // XXX probably sufficient if n1 can be completely embedded into n2,
  // XXX not necessarily at offset 0
  if (!n1.isArray () && n2.isArray ())
  {
    if (offset.getNumericOffset() > 0 && n1.size () + o > n2.size ())
      { m_sim.clear (); return false; }
  }

  // XXX: a collapsed node can simulate an array node
  if (n1.isArray () && (!n2.isArray () && !n2.isCollapsed()))
  { m_sim.clear (); return false; }

  if (n1.isArray () && offset.getNumericOffset() != 0)
  { m_sim.clear (); return false; } 
    
  // XXX: a collapsed node can simulate an array node
  if (n1.isArray () && !n2.isCollapsed() && n1.size () != n2.size ())
  { m_sim.clear (); return false; }
  
  if (n1.isCollapsed () && !n2.isCollapsed ())
  { m_sim.clear (); return false; }
      
  // At this point, n1 (at offset 0) is simulated by n2 at adjusted
  // offset. Thus, we add n2 into the map.
  map[&n2] = offset.getNumericOffset();

  // Check children recursively
  for (auto &kv : n1.links ())
  {
    unsigned j = n2.isCollapsed () ? 0 : kv.first.getOffset() +
                                         offset.getNumericOffset();
    if (!n2.hasLink(Field(j, FieldType::NotImplemented())))
    { m_sim.clear (); return false; }
    
    Node *n3 = kv.second->getNode ();
    // adjusted offset
    unsigned off1 = kv.second->getOffset ();

    auto &link = n2.getLink(Field(j, FieldType::NotImplemented()));
    Node *n4 = link.getNode ();
    // adjusted offset
    unsigned off2 = link.getOffset ();

    // Offsets must be adjusted 
    if (off2 < off1 && !n4->isCollapsed ())
    { m_sim.clear (); return false; }

    // Offsets must be adjusted     
    if (!insert (*n3, *n4, off2 - off1))
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
      // FIXME: Types
      auto res = inv_sim.insert(Cell(c.first, c.second,
                                     FieldType::NotImplemented()));
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
