#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"

#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/GraphTraits.hh"
#include "sea_dsa/Info.hh"
#include "sea_dsa/support/Debug.h"

/*
   Convert each DSA graph to .dot file.
 */

static llvm::cl::opt<std::string>
    OutputDir("sea-dsa-dot-outdir",
              llvm::cl::desc("DSA: output directory for dot files"),
              llvm::cl::init(""), llvm::cl::value_desc("DIR"));

using namespace llvm;

namespace sea_dsa {

namespace internals {

/* XXX: The API of llvm::GraphWriter is not flexible enough.
   We need the labels for source and destinations of edges to be the
   same rather than s0,s1... and d0,d1,..

   internals::GraphWriter is a copy of llvm::GraphWriter but with
   some changes (search for MODIFICATION) in writeEdge customized to
   output nice Dsa graphs.
*/

template <typename GraphType> class GraphWriter {
  raw_ostream &O;
  const GraphType &G;

  typedef DOTGraphTraits<GraphType> DOTTraits;
  typedef GraphTraits<GraphType> GTraits;
  typedef typename GTraits::NodeType NodeType;
  typedef typename GTraits::nodes_iterator node_iterator;
  typedef typename GTraits::ChildIteratorType child_iterator;
  DOTTraits DTraits;

  // Writes the edge labels of the node to O and returns true if there are any
  // edge labels not equal to the empty string "".
  bool getEdgeSourceLabels(raw_ostream &O, NodeType *Node) {
    child_iterator EI = GTraits::child_begin(Node);
    child_iterator EE = GTraits::child_end(Node);
    bool hasEdgeSourceLabels = false;

    for (unsigned i = 0; EI != EE && i != 64; ++EI, ++i) {
      std::string label = DTraits.getEdgeSourceLabel(Node, EI);

      if (label.empty())
        continue;

      hasEdgeSourceLabels = true;

      if (i)
        O << "|";

      O << "<s" << i << ">" << DOT::EscapeString(label);
    }

    if (EI != EE && hasEdgeSourceLabels)
      O << "|<s64>truncated...";


    return hasEdgeSourceLabels;
  }

public:
  GraphWriter(raw_ostream &o, const GraphType &g, bool SN) : O(o), G(g) {
    DTraits = DOTTraits(SN);
  }

  void writeGraph(const std::string &Title = "") {
    // Output the header for the graph...
    writeHeader(Title);

    // Emit all of the nodes in the graph...
    writeNodes();

    // Output any customizations on the graph
    DOTGraphTraits<GraphType>::addCustomGraphFeatures(G, *this);

    // Output the end of the graph
    writeFooter();
  }

  void writeHeader(const std::string &Title) {
    std::string GraphName = DTraits.getGraphName(G);

    if (!Title.empty())
      O << "digraph \"" << DOT::EscapeString(Title) << "\" {\n";
    else if (!GraphName.empty())
      O << "digraph \"" << DOT::EscapeString(GraphName) << "\" {\n";
    else
      O << "digraph unnamed {\n";

    if (DTraits.renderGraphFromBottomUp())
      O << "\trankdir=\"BT\";\n";

    if (!Title.empty())
      O << "\tlabel=\"" << DOT::EscapeString(Title) << "\";\n";
    else if (!GraphName.empty())
      O << "\tlabel=\"" << DOT::EscapeString(GraphName) << "\";\n";
    O << DTraits.getGraphProperties(G);
    O << "\n";
  }

  void writeFooter() {
    // Finish off the graph
    O << "}\n";
  }

  void writeNodes() {
    // Loop over the graph, printing it out...
    for (node_iterator I = GTraits::nodes_begin(G), E = GTraits::nodes_end(G);
         I != E; ++I)
      if (!isNodeHidden(*I))
        writeNode(*I);
  }

  bool isNodeHidden(NodeType &Node) { return isNodeHidden(&Node); }

  bool isNodeHidden(NodeType *const *Node) { return isNodeHidden(*Node); }

  bool isNodeHidden(NodeType *Node) { return DTraits.isNodeHidden(Node); }

  void writeNode(NodeType &Node) { writeNode(&Node); }

  void writeNode(NodeType *const *Node) { writeNode(*Node); }

  void writeNode(NodeType *Node) {
    std::string NodeAttributes = DTraits.getNodeAttributes(Node, G);

    O << "\tNode" << static_cast<const void *>(Node) << " [shape=record,";
    if (!NodeAttributes.empty())
      O << NodeAttributes << ",";
    O << "label=\"{";

    if (!DTraits.renderGraphFromBottomUp()) {
      O << DOT::EscapeString(DTraits.getNodeLabel(Node, G));

      // If we should include the address of the node in the label, do so now.
      std::string Id = DTraits.getNodeIdentifierLabel(Node, G);
      if (!Id.empty())
        O << "|" << DOT::EscapeString(Id);

      std::string NodeDesc = DTraits.getNodeDescription(Node, G);
      if (!NodeDesc.empty())
        O << "|" << DOT::EscapeString(NodeDesc);
    }

    std::string edgeSourceLabels;
    raw_string_ostream EdgeSourceLabels(edgeSourceLabels);
    bool hasEdgeSourceLabels = getEdgeSourceLabels(EdgeSourceLabels, Node);

    if (hasEdgeSourceLabels) {
      if (!DTraits.renderGraphFromBottomUp())
        O << "|";

      O << "{" << EdgeSourceLabels.str() << "}";

      if (DTraits.renderGraphFromBottomUp())
        O << "|";
    }

    if (DTraits.renderGraphFromBottomUp()) {
      O << DOT::EscapeString(DTraits.getNodeLabel(Node, G));

      // If we should include the address of the node in the label, do so now.
      std::string Id = DTraits.getNodeIdentifierLabel(Node, G);
      if (!Id.empty())
        O << "|" << DOT::EscapeString(Id);

      std::string NodeDesc = DTraits.getNodeDescription(Node, G);
      if (!NodeDesc.empty())
        O << "|" << DOT::EscapeString(NodeDesc);
    }

#if 0 
        // XXX: MODIFICATION
        // if (DTraits.hasEdgeDestLabels()) {
        // 	O << "|{";
	
        // 	unsigned i = 0, e = DTraits.numEdgeDestLabels(Node);
        // 	for (; i != e && i != 64; ++i) {
        // 	  if (i) O << "|";
        // 	  O << "<d" << i << ">"
        // 	    << DOT::EscapeString(DTraits.getEdgeDestLabel(Node, i));
        // 	}
	
        // 	if (i != e)
        // 	  O << "|<d64>truncated...";
        // 	O << "}";
        // }
#endif

    O << "}\"];\n"; // Finish printing the "node" line

    // Output all of the edges now
//    child_iterator EI = GTraits::child_begin(Node);
//    child_iterator EE = GTraits::child_end(Node);
//    for (unsigned i = 0; EI != EE && i != 64; ++EI, ++i)
//      if (!DTraits.isNodeHidden(*EI))
//        writeEdge(Node, i, EI);
//    for (; EI != EE; ++EI)
//      if (!DTraits.isNodeHidden(*EI))
//        writeEdge(Node, 64, EI);
  }

  /// emitSimpleNode - Outputs a simple (non-record) node
  void
  emitSimpleNode(const void *ID, const std::string &Attr,
                 const std::string &Label, unsigned NumEdgeSources = 0,
                 const std::vector<std::string> *EdgeSourceLabels = nullptr) {
    O << "\tNode" << ID << "[ ";
    if (!Attr.empty())
      O << Attr << ",";
    O << " label =\"";
    if (NumEdgeSources)
      O << "{";
    O << DOT::EscapeString(Label);
    if (NumEdgeSources) {
      O << "|{";

      for (unsigned i = 0; i != NumEdgeSources; ++i) {
        if (i)
          O << "|";
        O << "<s" << i << ">";
        if (EdgeSourceLabels)
          O << DOT::EscapeString((*EdgeSourceLabels)[i]);
      }
      O << "}}";
    }
    O << "\"];\n";
  }

  /// emitEdge - Output an edge from a simple node into the graph...
  void emitEdge(const void *SrcNodeID, int SrcNodePort, const void *DestNodeID,
                int DestNodePort, llvm::Twine Attrs) {
    if (SrcNodePort > 64)
      return; // Eminating from truncated part?
//    if (DestNodePort > 64)
//      DestNodePort = 64; // Targeting the truncated part?

    // Ignore DestNodePort and point to the whole node instead.
    DestNodePort = -1;

    O << "\tNode" << SrcNodeID;
    if (SrcNodePort >= 0)
      O << ":s" << SrcNodePort;
    O << " -> Node" << DestNodeID;

    // Edges that go to cells with zero offset do not
    // necessarily point to field 0. This makes graphs nicer.
    if (DestNodePort > 0 && DTraits.hasEdgeDestLabels()) {
      O << ":s" << DestNodePort;
    }

    if (!Attrs.isTriviallyEmpty())
      O << "[" << Attrs << "]";

    O << ";\n";
  }

  /// getOStream - Get the raw output stream into the graph file. Useful to
  /// write fancy things using addCustomGraphFeatures().
  raw_ostream &getOStream() { return O; }
};

template <typename GraphType>
raw_ostream &WriteGraph(raw_ostream &O, const GraphType &G,
                        bool ShortNames = false, const Twine &Title = "") {
  // Start the graph emission process...
  GraphWriter<GraphType> W(O, G, ShortNames);

  // Emit the graph.
  W.writeGraph(Title.str());
  return O;
}

} // namespace internals
} // end namespace sea_dsa

namespace llvm {

template <>
struct DOTGraphTraits<sea_dsa::Graph *> : public DefaultDOTGraphTraits {

  DOTGraphTraits(bool &b) {}
  DOTGraphTraits() {}

  // static std::string getGraphName(const sea_dsa::Graph *G) {
  // }

  static std::string getGraphProperties(const sea_dsa::Graph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    OS << "\tgraph [center=true, ratio=true, bgcolor=lightgray, "
          "fontname=Helvetica];\n";
    OS << "\tnode  [fontname=Helvetica, fontsize=11];\n";
    return OS.str();
  }

  static std::string getNodeAttributes(const sea_dsa::Node *N,
                                       const sea_dsa::Graph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    if (N->isOffsetCollapsed() && N->isTypeCollapsed()) {
      OS << "color=brown1, style=filled";
    } else if (N->isOffsetCollapsed()) {
      OS << "color=chocolate1, style=filled";
    } else if (N->isTypeCollapsed() && sea_dsa::IsTypeAware) {
      OS << "color=darkorchid2, style=filled";
    }
    return OS.str();
  }

  static std::string getNodeLabel(const sea_dsa::Node *N,
                                  const sea_dsa::Graph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    if (N->isForwarding()) {
      OS << "FORWARDING";
    } else {
      if (N->isOffsetCollapsed() || (N->isTypeCollapsed() &&
                                     sea_dsa::IsTypeAware)) {
        if (N->isOffsetCollapsed())
          OS << "OFFSET-";
        if (N->isTypeCollapsed() && sea_dsa::IsTypeAware)
          OS << "TYPE-";
        OS << "COLLAPSED";
      } else {
        // Go through all the types, and just print them.
        const auto &ts = N->types();
        bool firstType = true;
        OS << "{";
        if (ts.begin() != ts.end()) {
          for (auto ii = ts.begin(), ee = ts.end(); ii != ee; ++ii) {
            if (!firstType)
              OS << ",";
            firstType = false;
            OS << ii->first << ":"; // offset
            if (ii->second.begin() != ii->second.end()) {
              bool first = true;
              for (const Type *t : ii->second) {
                if (!first)
                  OS << "|";
                t->print(OS);
                first = false;
              }
            } else
              OS << "void";
          }
        } else {
          OS << "void";
        }
        OS << "}";
      }
      OS << ":";
      typename sea_dsa::Node::NodeType node_type = N->getNodeType();
      if (node_type.array)
        OS << " Sequence ";
      if (node_type.alloca)
        OS << "S";
      if (node_type.heap)
        OS << "H";
      if (node_type.global)
        OS << "G";
      if (node_type.unknown)
        OS << "U";
      if (node_type.incomplete)
        OS << "I";
      if (node_type.modified)
        OS << "M";
      if (node_type.read)
        OS << "R";
      if (node_type.external)
        OS << "E";
      if (node_type.externFunc)
        OS << "X";
      if (node_type.externGlobal)
        OS << "Y";
      if (node_type.inttoptr)
        OS << "P";
      if (node_type.ptrtoint)
        OS << "2";
      if (node_type.vastart)
        OS << "V";
      if (node_type.dead)
        OS << "D";
      // OS << " " << N->size() << " ";
    }

    return OS.str();
  }

  bool isNodeHidden(sea_dsa::Node *Node) {
    // TODO: do not show nodes without incoming edges
    return false;
  }

  static std::string getEdgeSourceLabel(const sea_dsa::Node *Node,
                                        sea_dsa::Node::iterator I) {
    std::string S;
    llvm::raw_string_ostream O(S);
    O << I.getField();
    O.flush();
    std::string Res;
    Res.reserve(S.size() + 2);

    for (char C : S) {
      if (C == '"')
        Res.push_back('\\');
      Res.push_back(C);
    }

    return Res;
  }

  static bool hasEdgeDestLabels() { return true; }

  static unsigned numEdgeDestLabels(const sea_dsa::Node *Node) {
    return Node->links().size();
  }

  ///////
  // XXX: if we use llvm::GraphWriter and we want to add destination labels.
  // It won't show the graphs as we want anyway.
  ///////
  // static std::string getEdgeDestLabel(const sea_dsa::Node *Node,
  // 					unsigned Idx) {
  //   std::string S;
  //   llvm::raw_string_ostream O(S);
  //   O << getOffset (Node,Idx);
  //   return O.str();
  // }

  // static bool edgeTargetsEdgeSource(const void *Node,
  // 				      sea_dsa::Node::iterator I) {
  //   if (I.getOffset() < I->getNode()->size()) {
  //   	if (I->hasLink (I.getOffset())) {
  // 	  //unsigned O = I->getLink(I.getOffset()).getOffset();
  // 	  //return O != 0;
  // 	  return true;
  // 	} else {
  // 	  return false;
  // 	}
  //   } else {
  //   	return false;
  //   }
  // }

  // static sea_dsa::Node::iterator getEdgeTarget(const sea_dsa::Node *Node,
  // 						      sea_dsa::Node::iterator I)
  // {
  //   unsigned O = I->getLink(I.getOffset()).getOffset();
  //   unsigned LinkNo = O;
  //   sea_dsa::Node *N = *I;
  //   sea_dsa::Node::iterator R = N->begin();
  //   for (; LinkNo; --LinkNo) ++R;
  //   return R;
  // }

  // static int getOffset (const sea_dsa::Node *Node, unsigned Idx) {
  //   auto it = Node->links().begin();
  //   auto et = Node->links().end();
  //   unsigned i = 0;
  //   for (; it!=et; ++it,i++) {
  // 	if (i==Idx) {
  // 	  return it->first;
  // 	}
  //   }
  //   return -1;
  // }

  static int getIndex(const sea_dsa::Node *Node, sea_dsa::Field Offset) {
    auto it = Node->links().begin();
    auto et = Node->links().end();
    unsigned idx = 0;
    for (; it != et; ++it, ++idx) {
      if (it->first == Offset)
        return idx;
    }
    return -1;
  }

  static void addCustomGraphFeatures(
      sea_dsa::Graph *g,
      sea_dsa::internals::GraphWriter<sea_dsa::Graph *> &GW) {

    typedef sea_dsa::Graph::scalar_const_iterator scalar_const_iterator;
    typedef sea_dsa::Graph::formal_const_iterator formal_const_iterator;
    typedef sea_dsa::Graph::return_const_iterator return_const_iterator;
    typedef sea_dsa::Node Node;
    typedef sea_dsa::Field Field;
    typedef sea_dsa::Graph::const_iterator node_const_iterator;

    auto EmitLinkTypeSuffix = [](const sea_dsa::Cell &C,
                                 sea_dsa::FieldType Ty =
                                                   FIELD_TYPE_NOT_IMPLEMENTED) {
      std::string Buff;
      llvm::raw_string_ostream OS(Buff);

      OS << ",label=\"" << C.getOffset();
      if (!Ty.isUnknown()) {
        OS << ", ";
        Ty.dump(OS);
      }

      OS << "\",fontsize=8";

      return OS.str();
    };



    // print edges from scalar (local and global) variables to cells
    {
      scalar_const_iterator it = g->scalar_begin();
      scalar_const_iterator et = g->scalar_end();
      for (; it != et; ++it) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        const llvm::Value *v = it->first;
        if (v->hasName())
          OS << v->getName();
        else
          OS << *v;
        GW.emitSimpleNode(it->first, "shape=plaintext", OS.str());
        Node *DestNode = it->second->getNode();
        Field DestField(it->second->getOffset(), FIELD_TYPE_NOT_IMPLEMENTED);
        int EdgeDest = getIndex(DestNode, DestField);
        GW.emitEdge(it->first, -1, DestNode, EdgeDest,
                    Twine("arrowtail=tee", EmitLinkTypeSuffix(*it->second)) +
                          ",color=purple");
      }
    }

    // print edges from call sites to cells
    //TODO: BUG: the callsite is drawn twice. One as a callsite and one as a value
    {
      for (auto &C : g->dsa_call_sites()) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        OS<<"callsite:";
        if(C.isIndirectCall()){
          const llvm::Value *v = C.getCallSite().getCalledValue();
          if(v->hasName())
            OS << v->getName();
          else
            OS << *v;
          LOG("dsa-callsite", errs()<<OS.str()<<"\n");

          GW.emitSimpleNode(v, "shape=egg", OS.str());
          Node *DestNode = g->getCell(*v).getNode();
          Field DestField(g->getCell(*v).getOffset(), FIELD_TYPE_NOT_IMPLEMENTED);
          int EdgeDest = getIndex(DestNode, DestField);
          GW.emitEdge(v, -1, DestNode, EdgeDest,
                      Twine("arrowtail=tee", EmitLinkTypeSuffix(g->getCell(*v))) +
                      ",color=green");
        }
      }
    }

    // print edges from formal parameters to cells
    {
      formal_const_iterator it = g->formal_begin();
      formal_const_iterator et = g->formal_end();
      for (; it != et; ++it) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        const llvm::Argument *arg = it->first;
        OS << arg->getParent()->getName() << "#" << arg->getArgNo();
        GW.emitSimpleNode(it->first, "shape=plaintext,fontcolor=blue",
                          OS.str());
        Node *DestNode = it->second->getNode();
        Field DestField(it->second->getOffset(), FIELD_TYPE_NOT_IMPLEMENTED);
        int EdgeDest = getIndex(DestNode, DestField);
        GW.emitEdge(it->first, -1, DestNode, EdgeDest,
                    Twine("tailclip=false,color=dodgerblue3",
                          EmitLinkTypeSuffix(*it->second)));
      }
    }

    // print edges from function return to cells
    {
      return_const_iterator it = g->return_begin();
      return_const_iterator et = g->return_end();
      for (; it != et; ++it) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        const llvm::Function *f = it->first;
        OS << f->getName() << "#Ret";
        GW.emitSimpleNode(it->first, "shape=plaintext,fontcolor=blue",
                          OS.str());
        Node *DestNode = it->second->getNode();
        Field DestField(it->second->getOffset(), FIELD_TYPE_NOT_IMPLEMENTED);
        int EdgeDest = getIndex(DestNode, DestField);
        GW.emitEdge(it->first, -1, DestNode, EdgeDest,
                    Twine("arrowtail=tee,color=gray63",
                          EmitLinkTypeSuffix(*it->second)));
      }
    }

    // print node edges
    {
      for (node_const_iterator it = g->begin(), e = g->end(); it != e; ++it) {
        const Node &N = *it;
        if (!N.isForwarding()) {

          for (auto &OffLink : N.getLinks()) {
            const int EdgeSrc = getIndex(&N, OffLink.first);
            const sea_dsa::Cell &C = *OffLink.second.get();
            const int EdgeDest = getIndex(C.getNode(), OffLink.first);
            GW.emitEdge(&N, EdgeSrc, C.getNode(), EdgeDest, Twine("arrowtail=tee",
                        EmitLinkTypeSuffix(C, OffLink.first.getType())));
          }

          continue;
        }

        const sea_dsa::Cell &Dest = N.getForwardDest();
        GW.emitEdge(&N, -1, Dest.getNode(), -1,
                    Twine("arrowtail=tee,color=dodgerblue1,style=dashed",
                          EmitLinkTypeSuffix(Dest)));
      }
    }
  }
};

} // end namespace llvm

namespace sea_dsa {

using namespace llvm;

static std::string appendOutDir(std::string FileName) {
  if (!OutputDir.empty()) {
    if (!llvm::sys::fs::create_directory(OutputDir)) {
      std::string FullFileName = OutputDir + "/" + FileName;
      return FullFileName;
    }
  }
  return FileName;
}

static bool writeGraph(Graph *G, std::string Filename) {
  std::string FullFilename = appendOutDir(Filename);
  std::error_code EC;
  raw_fd_ostream File(FullFilename, EC, sys::fs::F_Text);
  if (!EC) {
    internals::WriteGraph(File, G);
    LOG("dsa-printer", G->write(errs()));
    return true;
  }
  return false;
}

struct DsaPrinter : public ModulePass {
  static char ID;
  DsaAnalysis *m_dsa;

  DsaPrinter() : ModulePass(ID), m_dsa(nullptr) {}

  bool runOnModule(Module &M) override {
    m_dsa = &getAnalysis<sea_dsa::DsaAnalysis>();
    assert(m_dsa);

    if (m_dsa->getDsaAnalysis().kind() == CONTEXT_INSENSITIVE) {
      Function *main = M.getFunction("main");
      if (main && m_dsa->getDsaAnalysis().hasGraph(*main)) {
        Graph *G = &m_dsa->getDsaAnalysis().getGraph(*main);
        std::string Filename = main->getName().str() + ".mem.dot";
        writeGraph(G, Filename);
      }
    } else {
      for (auto &F : M)
        runOnFunction(F);
    }
    return false;
  }

  bool runOnFunction(Function &F) {
    if (m_dsa->getDsaAnalysis().hasGraph(F)) {
      Graph *G = &m_dsa->getDsaAnalysis().getGraph(F);
      if (G->begin() != G->end()) {
        std::string Filename = F.getName().str() + ".mem.dot";
        writeGraph(G, Filename);
      }
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<DsaAnalysis>();
  }

  StringRef getPassName() const override 
  { return "SeaHorn Dsa graph printer"; }
	  
};

// Used by Graph::viewGraph() and Node::viewGraph().
void ShowDsaGraph(Graph& G) {
  static unsigned I = 0;
  const std::string Filename = "temp" + std::to_string(I++) + ".mem.dot";
  const bool Res = writeGraph(&G, Filename);
  (void)Res;
  assert(Res && "Could not write graph");
  DisplayGraph(appendOutDir(Filename), /* wait = */ false, GraphProgram::DOT);
}

struct DsaViewer : public ModulePass {
  static char ID;
  DsaAnalysis *m_dsa;
  const bool wait;

  DsaViewer() : ModulePass(ID), m_dsa(nullptr), wait(false) {}

  bool runOnModule(Module &M) override {
    m_dsa = &getAnalysis<sea_dsa::DsaAnalysis>();
    if (m_dsa->getDsaAnalysis().kind() == CONTEXT_INSENSITIVE) {
      Function *main = M.getFunction("main");
      if (main && m_dsa->getDsaAnalysis().hasGraph(*main)) {
        Graph *G = &m_dsa->getDsaAnalysis().getGraph(*main);
        std::string Filename = main->getName().str() + ".mem.dot";
        if (writeGraph(G, Filename))
          DisplayGraph(Filename, wait, GraphProgram::DOT);
      }
    } else {
      for (auto &F : M)
        runOnFunction(F);
    }
    return false;
  }

  bool runOnFunction(Function &F) {
    if (m_dsa->getDsaAnalysis().hasGraph(F)) {
      Graph *G = &m_dsa->getDsaAnalysis().getGraph(F);
      if (G->begin() != G->end()) {
        std::string Filename = F.getName().str() + ".mem.dot";
        if (writeGraph(G, Filename)) {
          DisplayGraph(Filename, wait, GraphProgram::DOT);
        }
      }
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<DsaAnalysis>();
  }

  StringRef getPassName() const override 
  { return "SeaHorn Dsa graph viewer"; }

};

char DsaPrinter::ID = 0;
char DsaViewer::ID = 0;

Pass *createDsaPrinterPass() { return new sea_dsa::DsaPrinter(); }

Pass *createDsaViewerPass() { return new sea_dsa::DsaViewer(); }

} // end namespace sea_dsa

static llvm::RegisterPass<sea_dsa::DsaPrinter> X("seadsa-printer",
                                                 "Print memory graphs");

static llvm::RegisterPass<sea_dsa::DsaViewer> Y("seadsa-viewer",
                                                "View memory graphs");
