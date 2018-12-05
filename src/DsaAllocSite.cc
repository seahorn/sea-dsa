#include "sea_dsa/AllocSite.hh"

namespace sea_dsa {

void DSAllocSite::print(llvm::raw_ostream &os) const {
  if (m_allocSite)
    m_allocSite->print(os);
  else
    os << "<nullptr>";

  for (const auto& Path : m_callPaths) {
    os << "\n\t(";
    for (const Step &step : Path) {
      if (step.first == Local) {
        os << "LOCAL ";
      } else if (step.first == BottomUp) {
        os << " -BU-> ";
      } else if (step.first == TopDown) {
        os << " <-TD- ";
      }
    }
    os << ")\n";
  }
}

}