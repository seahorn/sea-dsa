#pragma once
#include <string>
#include <set>

#define LOG(TAG,CODE) \
  do { if(::sea_dsa::SeaDsaLog.count(TAG) > 0) {CODE;} else {;} } while(false)


namespace sea_dsa {

  extern std::set<std::string> SeaDsaLog;
  void SeaDsaEnableLog (std::string x);

  struct SeaDsaLogOpt {
    void operator=(const std::string &tag) const {
      SeaDsaEnableLog(tag);
    }
  };
}

