#pragma once

#include "parsed-derivations.hh"
#include "local-store.hh"

namespace nix {

typedef std::map<std::string, std::string> StringRewrites;
typedef std::map<Path, Path> PathMap;
typedef std::map<std::string, Path> OutLink;

bool rebuildDrvForCasInputs(LocalStore* store, BasicDerivation* drv, OutLink &wantedAliases);
std::string rewriteStrings(std::string s, const StringRewrites & rewrites);

}
