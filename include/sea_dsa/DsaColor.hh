#pragma once

#include "llvm/ADT/DenseMap.h"

#include "sea_dsa/CallSite.hh"

using namespace sea_dsa;
using namespace llvm;

namespace sea_dsa {

using Color = unsigned;

class Graph;
class Node;

using ColorMap = DenseMap<const Node *, Color>;
using NodeSet = DenseSet<const Node *>;

void colorGraph(const DsaCallSite &cs, const Graph &calleeG,
                const Graph &callerG, ColorMap &color_callee,
                ColorMap &color_caller);

} // end namespace sea_dsa
