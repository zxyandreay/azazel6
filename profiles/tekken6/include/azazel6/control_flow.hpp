#pragma once
#include "psprecomp/runtime.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <initializer_list>

namespace azazel6 {
inline bool guard_indirect_call(psprecomp::Runtime& rt, std::uint32_t site, std::uint32_t target,
                                std::initializer_list<std::uint32_t> allowed) {
    if (std::find(allowed.begin(), allowed.end(), target) != allowed.end()) return true;
    rt.stop("Unverified indirect target " + psprecomp::hex32(target) + " at " + psprecomp::hex32(site));
    return false;
}
}
