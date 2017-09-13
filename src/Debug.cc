#include <sea_dsa/support/Debug.h>

std::set<std::string> sea_dsa::SeaDsaLog;

void sea_dsa::SeaDsaEnableLog (std::string x) {
  if (x.empty()) return;
  sea_dsa::SeaDsaLog.insert (x); 
}





