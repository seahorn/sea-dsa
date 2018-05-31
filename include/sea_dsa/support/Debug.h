#ifndef __SEA_DSA_DEBUG__HPP_
#define __SEA_DSA_DEBUG__HPP_
#include <string>
#include <set>

#define LOG(TAG,CODE) \
  do { if (::sea_dsa::SeaDsaLog.count(TAG) > 0 || \
           ::sea_dsa::SeaDsaLog.count("all") > 0) { CODE; } } while (0)


namespace sea_dsa {

  extern std::set<std::string> SeaDsaLog;
  void SeaDsaEnableLog (std::string x);

  struct SeaDsaLogOpt {
    void operator=(const std::string &tag) const {
      SeaDsaEnableLog (tag);
    }
  };
  
}

#endif 
