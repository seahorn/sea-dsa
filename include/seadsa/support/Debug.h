#pragma once
#include <string>
#include <set>

#define LOG(TAG,CODE) \
  do { if(::seadsa::SeaDsaLog.count(TAG) > 0) {CODE;} else {;} } while(false)


namespace seadsa {

  extern std::set<std::string> SeaDsaLog;
  void SeaDsaEnableLog (std::string x);

  struct SeaDsaLogOpt {
    void operator=(const std::string &tag) const {
      SeaDsaEnableLog(tag);
    }
  };
}

