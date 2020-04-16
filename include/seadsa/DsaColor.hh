#pragma once

#include "llvm/ADT/DenseMap.h"

#include "seadsa/CallSite.hh"

using namespace seadsa;
using namespace llvm;

namespace seadsa {

using Color = unsigned;

class Graph;
class Node;

using ColorMap = DenseMap<const Node *, Color>;
using NodeSet = DenseSet<const Node *>;

void colorGraph(const DsaCallSite &cs, const Graph &calleeG,
                const Graph &callerG, ColorMap &color_callee,
                ColorMap &color_caller);

} // end namespace seadsa
