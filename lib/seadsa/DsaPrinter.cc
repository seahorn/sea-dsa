#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"

#include "seadsa/CompleteCallGraph.hh"
#include "seadsa/CallGraphUtils.hh"
#include "seadsa/DsaAnalysis.hh"
#include "seadsa/Global.hh"
#include "seadsa/GraphTraits.hh"
#include "seadsa/Info.hh"
#include "seadsa/support/Debug.h"
#include "seadsa/DsaColor.hh"

/*
   Convert each DSA graph to .dot file.
 */

namespace seadsa {
std::string DotOutputDir;
}

static llvm::cl::opt<std::string, true>
    XDotOutputDir("sea-dsa-dot-outdir",
                  llvm::cl::desc("Output directory for dot files"),
                  llvm::cl::location(seadsa::DotOutputDir), llvm::cl::init(""),
                  llvm::cl::value_desc("DIR"));

static llvm::cl::opt<bool> DsaColorCallSiteSimDot(
    "sea-dsa-color-callsite-sim-dot",
    llvm::cl::desc("Output colored graphs according to how nodes of callees "
                   "are simulated in the caller"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> DsaColorFunctionSimDot(
    "sea-dsa-color-func-sim-dot",
    llvm::cl::desc("Output colored graphs according to how nodes of a summary "
                   "graph are simulated in the final graph"),
    llvm::cl::init(false));

static llvm::cl::opt<bool>
    PrintAllocSites("sea-dsa-dot-print-as",
                    llvm::cl::desc("Print allocation sites for SeaDsa nodes"),
                    llvm::cl::init(false));

using namespace llvm;

namespace seadsa {

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
  ColorMap * m_cm = nullptr;

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
  GraphWriter(raw_ostream &o, const GraphType &g, bool SN, ColorMap *cm = nullptr ) : O(o), G(g), m_cm(cm) {
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
    std::string NodeAttributes = DTraits.getNodeAttributes(Node, G, m_cm);

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

    O << "}\"];\n"; // Finish printing the "node" line
  }

  /// emitSimpleNode - Outputs a simple (non-record) node
  void
  emitSimpleNode(const void *ID, const std::string &Attr,
                 const std::string &Label, unsigned NumEdgeSources = 0,
                 const std::vector<std::string> *EdgeSourceLabels = nullptr) {
    O << "\tNode" << ID << " [";
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
                        ColorMap *cm, bool ShortNames = false,
                        const Twine &Title = "") {
  // Start the graph emission process...
  GraphWriter<GraphType> W(O, G, ShortNames,cm);

  // Emit the graph.
  W.writeGraph(Title.str());
  return O;
}

} // namespace internals
} // end namespace seadsa

namespace llvm {

template <>
struct DOTGraphTraits<seadsa::Graph *> : public DefaultDOTGraphTraits {

  DOTGraphTraits(bool &b) {}
  DOTGraphTraits() {}

  // static std::string getGraphName(const seadsa::Graph *G) {
  // }

  static std::string getGraphProperties(const seadsa::Graph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    OS << "\tgraph [center=true, ratio=true, bgcolor=lightgray, "
          "fontname=Helvetica];\n";
    OS << "\tnode  [fontname=Helvetica, fontsize=11];\n";
    return OS.str();
  }

  static std::string getNodeAttributes(const seadsa::Node *N,
                                       const seadsa::Graph *G, ColorMap *cm) {
    std::string empty;
    raw_string_ostream OS(empty);

    if (cm != nullptr) {
      auto it = cm->find(N);
      if (it != cm->end()) {
        std::ostringstream stringStream;
        Color c = it->getSecond();
        stringStream << "\"#" << std::hex << c
                     << "\""; // this can be done because we know c > 0x808080
        OS << "fillcolor=" << stringStream.str() << ", style=filled";
      } else
        OS << "fillcolor=gray, style=filled";
    } else {
      if (N->isOffsetCollapsed() && N->isTypeCollapsed()) {
        OS << "fillcolor=brown1, style=filled";
      } else if (N->isOffsetCollapsed()) {
        OS << "fillcolor=chocolate1, style=filled";
      } else if (N->isTypeCollapsed() && seadsa::g_IsTypeAware) {
        OS << "fillcolor=darkorchid2, style=filled";
      } else {
        OS << "fillcolor=gray, style=filled";
      }
    }
    return OS.str();
  }

  static void writeAllocSites(const seadsa::Node &Node,
                              llvm::raw_string_ostream &O) {
    O << "\nAS:";
    const size_t numAllocSites = Node.getAllocSites().size();
    size_t i = 0;
    for (const llvm::Value *AS : Node.getAllocSites()) {
      assert(AS);
      ++i;
      O << " " << DOT::EscapeString(AS->hasName() ? AS->getName().str() : "unnamed");
      if (i != numAllocSites)
        O << ",";
    }
    O << "\n";
  }

  static std::string getNodeLabel(const seadsa::Node *N,
                                  const seadsa::Graph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    if (N->isForwarding()) {
      OS << "FORWARDING";
    } else {
      if (N->isOffsetCollapsed() || (N->isTypeCollapsed() &&
                                     seadsa::g_IsTypeAware)) {
        if (N->isOffsetCollapsed())
          OS << "OFFSET-";
        if (N->isTypeCollapsed() && seadsa::g_IsTypeAware)
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
      typename seadsa::Node::NodeType node_type = N->getNodeType();
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
    }

    if (PrintAllocSites)
      writeAllocSites(*N, OS);

    return OS.str();
  }

  bool isNodeHidden(seadsa::Node *Node) {
    // TODO: do not show nodes without incoming edges
    return false;
  }

  static std::string getEdgeSourceLabel(const seadsa::Node *Node,
                                        seadsa::Node::iterator I) {
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

  static unsigned numEdgeDestLabels(const seadsa::Node *Node) {
    return Node->links().size();
  }

  ///////
  // XXX: if we use llvm::GraphWriter and we want to add destination labels.
  // It won't show the graphs as we want anyway.
  ///////
  // static std::string getEdgeDestLabel(const seadsa::Node *Node,
  // 					unsigned Idx) {
  //   std::string S;
  //   llvm::raw_string_ostream O(S);
  //   O << getOffset (Node,Idx);
  //   return O.str();
  // }

  // static bool edgeTargetsEdgeSource(const void *Node,
  // 				      seadsa::Node::iterator I) {
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

  // static seadsa::Node::iterator getEdgeTarget(const seadsa::Node *Node,
  // 						      seadsa::Node::iterator I)
  // {
  //   unsigned O = I->getLink(I.getOffset()).getOffset();
  //   unsigned LinkNo = O;
  //   seadsa::Node *N = *I;
  //   seadsa::Node::iterator R = N->begin();
  //   for (; LinkNo; --LinkNo) ++R;
  //   return R;
  // }

  // static int getOffset (const seadsa::Node *Node, unsigned Idx) {
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

  static int getIndex(const seadsa::Node *Node, seadsa::Field Offset) {
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
      seadsa::Graph *g,
      seadsa::internals::GraphWriter<seadsa::Graph *> &GW) {

    typedef seadsa::Node Node;
    typedef seadsa::Field Field;

    auto EmitLinkTypeSuffix = [](const seadsa::Cell &C,
                                 seadsa::FieldType Ty =
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
      for (const auto &scalar : g->scalars()) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        const llvm::Value *v = scalar.first;
        if (v->hasName())
          OS << v->getName();
        else
          OS << *v;
        GW.emitSimpleNode(scalar.first, "shape=plaintext", OS.str());
        Node *DestNode = scalar.second->getNode();
        Field DestField(scalar.second->getOffset(), FIELD_TYPE_NOT_IMPLEMENTED);
        int EdgeDest = getIndex(DestNode, DestField);
        GW.emitEdge(scalar.first, -1, DestNode, EdgeDest,
                    Twine("arrowtail=tee", EmitLinkTypeSuffix(*scalar.second)) +
                        ",color=purple");
      }
    }

    // print edges from formal parameters to cells
    {
      for (const auto &formal : g->formals()) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        const llvm::Argument *arg = formal.first;
        OS << arg->getParent()->getName() << "#" << arg->getArgNo();
        GW.emitSimpleNode(formal.first, "shape=plaintext,fontcolor=blue",
                          OS.str());
        Node *DestNode = formal.second->getNode();
        Field DestField(formal.second->getOffset(), FIELD_TYPE_NOT_IMPLEMENTED);
        int EdgeDest = getIndex(DestNode, DestField);
        GW.emitEdge(formal.first, -1, DestNode, EdgeDest,
                    Twine("tailclip=false,color=dodgerblue3",
                          EmitLinkTypeSuffix(*formal.second)));
      }
    }

    // print edges from function return to cells
    {
      for (const auto &retNode : g->returns()) {
        std::string OS_str;
        llvm::raw_string_ostream OS(OS_str);
        const llvm::Function *f = retNode.first;
        OS << f->getName() << "#Ret";
        GW.emitSimpleNode(retNode.first, "shape=plaintext,fontcolor=blue",
                          OS.str());
        Node *DestNode = retNode.second->getNode();
        Field DestField(retNode.second->getOffset(),
                        FIELD_TYPE_NOT_IMPLEMENTED);
        int EdgeDest = getIndex(DestNode, DestField);
        GW.emitEdge(retNode.first, -1, DestNode, EdgeDest,
                    Twine("arrowtail=tee,color=gray63",
                          EmitLinkTypeSuffix(*retNode.second)));
      }
    }

    // print node edges
    {
      for (const Node &N : *g) {
        if (!N.isForwarding()) {

          for (auto &OffLink : N.getLinks()) {
            const int EdgeSrc = getIndex(&N, OffLink.first);
            const seadsa::Cell &C = *OffLink.second.get();
            const int EdgeDest = getIndex(C.getNode(), OffLink.first);
            GW.emitEdge(&N, EdgeSrc, C.getNode(), EdgeDest,
                        Twine("arrowtail=tee",
                              EmitLinkTypeSuffix(C, OffLink.first.getType())));
          }

          continue;
        }

        const seadsa::Cell &Dest = N.getForwardDest();
        GW.emitEdge(&N, -1, Dest.getNode(), -1,
                    Twine("arrowtail=tee,color=dodgerblue1,style=dashed",
                          EmitLinkTypeSuffix(Dest)));
      }
    }
  }
};

} // end namespace llvm

namespace seadsa {

using namespace llvm;

static std::string appendOutDir(std::string FileName) {
  if (!DotOutputDir.empty()) {
    if (!llvm::sys::fs::create_directory(DotOutputDir)) {
      std::string FullFileName = DotOutputDir + "/" + FileName;
      return FullFileName;
    }
  }
  return FileName;
}

static bool writeGraph(Graph *G, std::string Filename, ColorMap * cm = nullptr) {
  std::string FullFilename = appendOutDir(Filename);
  std::error_code EC;
  raw_fd_ostream File(FullFilename, EC, sys::fs::F_Text);
  if (!EC) {
    internals::WriteGraph(File, G, cm);
    LOG("dsa-printer", G->write(errs()));
    return true;
  }
  return false;
}

struct DsaPrinter : public ModulePass {
  static char ID;
  DsaAnalysis *m_dsa;

  DsaPrinter() : ModulePass(ID), m_dsa(nullptr) {}

private:
  int m_cs_count = 0; // to distinguish callsites

public :

  bool runOnModule(Module &M) override {
    m_dsa = &getAnalysis<seadsa::DsaAnalysis>();
    assert(m_dsa);

    if (m_dsa->getDsaAnalysis().kind() == GlobalAnalysisKind::CONTEXT_INSENSITIVE) {
      Function *main = M.getFunction("main");
      if (main && m_dsa->getDsaAnalysis().hasGraph(*main)) {
        Graph *G = &m_dsa->getDsaAnalysis().getGraph(*main);
        std::string Filename = main->getName().str() + ".mem.dot";
        writeGraph(G, Filename);
      }
    } else {
      if(DsaColorCallSiteSimDot){
        CompleteCallGraph &ccg = getAnalysis<CompleteCallGraph>();
        llvm::CallGraph &cg = ccg.getCompleteCallGraph();
        for (auto it = scc_begin(&cg); !it.isAtEnd(); ++it) {
          auto &scc = *it;
          for (CallGraphNode *cgn : scc) {
            Function *fn = cgn->getFunction();
            if (!fn || fn->isDeclaration() || fn->empty()) {
              continue;
            }
            // -- store the simulation maps from the SCC
            for (auto &callRecord : *cgn) {

              llvm::Optional<DsaCallSite> dsaCS = call_graph_utils::getDsaCallSite(callRecord);
              if (!dsaCS.hasValue()) {
                continue;
              }
              DsaCallSite &cs = dsaCS.getValue();
              Function * f_caller = fn;
              const Function * f_callee = cs.getCallee();
              Graph &callerG =
                m_dsa->getDsaAnalysis().getGraph(*f_caller);
              Graph &calleeG =
                m_dsa->getDsaAnalysis().getSummaryGraph(*f_callee);

              ColorMap color_callee, color_caller;
              colorGraphsCallSite(cs, calleeG, callerG, color_callee,
                                  color_caller);
              std::string FilenameBase =
                f_caller->getParent()->getModuleIdentifier() + "." +
                f_caller->getName().str() + "." + f_callee->getName().str() +
                "." + std::to_string(++m_cs_count);

              writeGraph(&calleeG, FilenameBase + ".callee.mem.dot",
                         &color_callee);
              writeGraph(&callerG, FilenameBase + ".caller.mem.dot",
                         &color_caller);

            }
          }
        }
      }
      for (auto &F : M)
        runOnFunction(F);
    }
    return false;
  }

  bool runOnFunction(Function &F) {
    GlobalAnalysis &ga = m_dsa->getDsaAnalysis();
    if (ga.hasGraph(F)) {
      Graph *G = &ga.getGraph(F);
      if (G->begin() != G->end()) {
        std::string Filename = F.getName().str() + ".mem.dot";
        writeGraph(G, Filename);
        if (DsaColorFunctionSimDot) { // simulate bu and sas graph and color
          if (ga.hasSummaryGraph(F)) {
            Graph *buG = &ga.getSummaryGraph(F);
            ColorMap colorBuG, colorG;
            colorGraphsFunction(F, *buG, *G, colorBuG, colorG);

            writeGraph(buG, F.getName().str() + ".BU.mem.dot", &colorBuG);
            writeGraph(G, F.getName().str() + ".TD.mem.dot", &colorG);
          }
        }
      }
    }

    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<DsaAnalysis>();
    if (DsaColorCallSiteSimDot)
      AU.addRequired<CompleteCallGraph>();
  }

  StringRef getPassName() const override { return "SeaHorn Dsa graph printer"; }
};

// Used by Graph::writeGraph().
void WriteDsaGraph(Graph& G, const std::string &filename) {
  const bool Res = writeGraph(&G, filename);
  (void)Res;
  assert(Res && "Could not write graph");
}

// Used by Graph::viewGraph() and Node::viewGraph().
void ShowDsaGraph(Graph& G) {
  static unsigned I = 0;
  const std::string Filename = "temp" + std::to_string(I++) + ".mem.dot";
  WriteDsaGraph(G, Filename);
  DisplayGraph(appendOutDir(Filename), /* wait = */ false, GraphProgram::DOT);
}

struct DsaViewer : public ModulePass {
  static char ID;
  DsaAnalysis *m_dsa;
  const bool wait;

  DsaViewer() : ModulePass(ID), m_dsa(nullptr), wait(false) {}

  bool runOnModule(Module &M) override {
    m_dsa = &getAnalysis<seadsa::DsaAnalysis>();
    if (m_dsa->getDsaAnalysis().kind() == GlobalAnalysisKind::CONTEXT_INSENSITIVE) {
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
  { return "SeaDsa graph viewer"; }

};

char DsaPrinter::ID = 0;
char DsaViewer::ID = 0;

Pass *createDsaPrinterPass() { return new seadsa::DsaPrinter(); }

Pass *createDsaViewerPass() { return new seadsa::DsaViewer(); }

} // end namespace seadsa

static llvm::RegisterPass<seadsa::DsaPrinter> X("seadsa-printer",
                                                 "Print SeaDsa memory graphs");

static llvm::RegisterPass<seadsa::DsaViewer> Y("seadsa-viewer",
                                                "View SeaDsa memory graphs");
