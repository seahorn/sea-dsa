#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/GraphWriter.h"

#include "sea_dsa/CompleteCallGraph.hh"
#include "sea_dsa/DsaAnalysis.hh"
#include "sea_dsa/Global.hh"
#include "sea_dsa/GraphTraits.hh"
#include "sea_dsa/Info.hh"
#include "sea_dsa/support/Debug.h"

#include "sea_dsa/DsaColor.hh"

namespace sea_dsa {
extern std::string DotOutputDir;
}

// static llvm::cl::opt<std::string, true>
// XDotOutputDir("sea-dsa-dot-outdir",
// 	   llvm::cl::desc("Output directory for dot files"),
// 	   llvm::cl::location(sea_dsa::DotOutputDir),
// 	   llvm::cl::init(""), llvm::cl::value_desc("DIR"));

// static llvm::cl::opt<bool>
//     PrintAllocSites("sea-dsa-dot-print-as",
//                     llvm::cl::desc("Print allocation sites for SeaDsa nodes"),
//                     llvm::cl::init(false));
using namespace llvm;

namespace sea_dsa {

namespace internals {

template <typename GraphType> class ColoredGraphWriter {
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
  ColoredGraphWriter(raw_ostream &o, const GraphType &g, bool SN) : O(o), G(g) {
    DTraits = DOTTraits(SN);
  }

  void writeColoredGraph(const std::string &Title = "") {
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
raw_ostream &WriteColoredGraph(raw_ostream &O, const GraphType &G,
                        bool ShortNames = false, const Twine &Title = "") {
  // Start the graph emission process...
  ColoredGraphWriter<GraphType> W(O, G, ShortNames);

  // Emit the graph.
  W.writeColoredGraph(Title.str());
  return O;
}

} // namespace internals
} // end namespace sea_dsa


namespace llvm {

template <>
struct DOTGraphTraits<sea_dsa::ColoredGraph *> : public DefaultDOTGraphTraits {

  DOTGraphTraits(bool &b) {}
  DOTGraphTraits() {}

  // static std::string getGraphName(const sea_dsa::Graph *G) {
  // }

  static std::string getGraphProperties(const sea_dsa::ColoredGraph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    OS << "\tgraph [center=true, ratio=true, bgcolor=lightgray, "
          "fontname=Helvetica];\n";
    OS << "\tnode  [fontname=Helvetica, fontsize=11];\n";
    return OS.str();
  }

  static std::string getNodeAttributes(const sea_dsa::Node *N,
                                       const sea_dsa::ColoredGraph *G) {
    std::string empty;
    raw_string_ostream OS(empty);
    OS << "fillcolor=" << G->getColorNode(N) << ", style=filled";

    if (G->isSafeNode(N)) {
      OS << ",color=green"; // This is the color for the border
    }
    return OS.str();
  }

  static void writeAllocSites(const sea_dsa::Node &Node,
                              llvm::raw_string_ostream &O) {
    O << "\nAS:";
    const size_t numAllocSites = Node.getAllocSites().size();
    size_t i = 0;
    for (const llvm::Value *AS : Node.getAllocSites()) {
      assert(AS);
      ++i;
      O << " " << DOT::EscapeString(AS->hasName() ? AS->getName() : "unnamed");
      if (i != numAllocSites)
        O << ",";
    }
    O << "\n";
  }

  static std::string getNodeLabel(const sea_dsa::Node *N,
                                  const sea_dsa::ColoredGraph *G) {
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
    }

    // if (PrintAllocSites)
    //   writeAllocSites(*N, OS);

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
      sea_dsa::ColoredGraph *g,
      sea_dsa::internals::ColoredGraphWriter<sea_dsa::ColoredGraph *> &GW) {

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
      for (const auto &scalar : g->getGraph().scalars()) {
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
      for (const auto &formal : g->getGraph().formals()) {
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
      for (const auto &retNode : g->getGraph().returns()) {
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
      for (const Node &N : g->getGraph()) {
        if (!N.isForwarding()) {

          for (auto &OffLink : N.getLinks()) {
            const int EdgeSrc = getIndex(&N, OffLink.first);
            const sea_dsa::Cell &C = *OffLink.second.get();
            const int EdgeDest = getIndex(C.getNode(), OffLink.first);
            GW.emitEdge(&N, EdgeSrc, C.getNode(), EdgeDest,
                        Twine("arrowtail=tee",
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

static std::string appendColorOutDir(std::string FileName) {
  if (!DotOutputDir.empty()) {
    if (!llvm::sys::fs::create_directory(DotOutputDir)) {
      std::string FullFileName = DotOutputDir + "/" + FileName;
      return FullFileName;
    }
  }
  return FileName;
}

static bool writeColoredGraph(ColoredGraph *G, std::string Filename) {
  std::string FullFilename = appendColorOutDir(Filename);
  std::error_code EC;
  raw_fd_ostream File(FullFilename, EC, sys::fs::F_Text);
  if (!EC) {
    internals::WriteColoredGraph(File, G);
    LOG("dsa-color-printer", G->getGraph().write(errs()));
    return true;
  }
  return false;
}

struct DsaColorPrinter : public ModulePass {

  static char ID;
  ContextSensitiveGlobalAnalysis *m_dsa;

  DsaColorPrinter() : ModulePass(ID), m_dsa(nullptr) {}

  bool runOnModule(Module &M) override {

    auto &CCG = getAnalysis<CompleteCallGraph>();
    auto &dsaCallGraph = CCG.getCompleteCallGraph();

    int count = 0; // to distinguish callsites

    CallGraphWrapper dsaCG(dsaCallGraph);

    dsaCG.buildDependencies(); // TODO: this is already done already in
                               // ContextSensitiveGlobalAnalysis but it is
                               // stored locally in the runOnModule function, so
                               // we have to recompute it.

    m_dsa = &getAnalysis<sea_dsa::ContextSensitiveGlobalPass>().getCSGlobalAnalysis();

    for (auto &F : M) { // F is the callee, and we are iterating over its callers
      if(!m_dsa->hasGraph(F))
        continue;

      auto call_sites = dsaCG.getUses(F);

      // call_sites is NULL?
      auto it = call_sites.begin();
      auto end = call_sites.end();

      for( ; it != end ; it++){

        ColorMap color_callee, color_caller;
        SafeNodeSet f_node_safe;

        const Function * f_caller = it->getCallSite().getCaller();

        Graph &callerG = m_dsa->getGraph(*f_caller);
        Graph &calleeG = m_dsa->getSummaryGraph(F);

        GraphExplorer::colorGraph(*it, calleeG, callerG, color_callee, color_caller,
                              f_node_safe); // colors the graphs

        std::string FilenameBase =
          M.getModuleIdentifier() + "." + f_caller->getName().str() +
          "." + F.getName().str() + "." + std::to_string(++count);

        ColoredGraph colorGCallee(calleeG, color_callee, f_node_safe);
        writeColoredGraph(&colorGCallee, FilenameBase + ".callee.mem.dot");

        ColoredGraph colorGCaller(callerG, color_caller, f_node_safe);
        writeColoredGraph(&colorGCaller, FilenameBase + ".caller.mem.dot");
      }
    }
    return false;
  }

  //  bool runOnFunction(Module &M) { return false; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<CompleteCallGraph>();
    AU.addRequired<ContextSensitiveGlobalPass>();
  }

  StringRef getPassName() const override
  { return "SeaHorn Dsa colored graph printer"; }

};

char DsaColorPrinter::ID = 0;
Pass *createDsaColorPrinterPass() { return new sea_dsa::DsaColorPrinter(); }

} // end namespace sea_dsa

static llvm::RegisterPass<sea_dsa::DsaColorPrinter> X("seadsa-color-printer",
                                                 "Print SeaDsa colored memory graphs");
