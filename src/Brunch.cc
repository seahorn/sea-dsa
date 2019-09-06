#include "sea_dsa/support/Brunch.hh"

#include <sys/resource.h>
#include <sys/time.h>

namespace sea_dsa {
long BrunchTimer::getCurrentTimeMsSteady() {
  rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  long r = ru.ru_utime.tv_sec * 1000L + ru.ru_utime.tv_usec / 1000;
  return r;
}

long GetCurrentMemoryUsageKb() {
  rusage ru;
  getrusage(RUSAGE_SELF, &ru);
  return ru.ru_maxrss;
}
} // namespace sea_dsa
