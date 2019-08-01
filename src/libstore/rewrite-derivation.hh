#pragma once

#include "parsed-derivations.hh"
#include "local-store.hh"

namespace nix {

typedef std::map<std::string, std::string> StringRewrites;

bool rebuildDrvForCasInputs(LocalStore* store, BasicDerivation* drv);
std::string rewriteStrings(std::string s, const StringRewrites & rewrites);

}
