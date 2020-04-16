#include <seadsa/support/Debug.h>

std::set<std::string> seadsa::SeaDsaLog;

void seadsa::SeaDsaEnableLog (std::string x) {
  if (x.empty()) return;
  seadsa::SeaDsaLog.insert (x); 
}





