#ifndef OWNSEM_H_
#define OWNSEM_H_

#include <boost/hana.hpp>

constexpr auto OWNSEM_BUILTIN_NAMES = boost::hana::make_set(
    "sea.mkown", "sea.mkshr", "sea.bor_mkbor", "sea.bor_ptr", "sea.bor_mksuc",
    "sea.begin_unique", "sea.end_unique",
    // "sea.die", // This is not a ptr def operation
    "sea.mov",
    // Builtin for fat ptr (slot0 and slot1 only)
    "sea.set_fatptr_slot", "sea.get_fatptr_slot", "sea.bor_mem2reg",
    "sea.mov_mem2reg", "sea.bor_mkbor_part");

#endif // OWNSEM_H_
