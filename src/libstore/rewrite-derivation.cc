#include "local-store.hh"
#include "parsed-derivations.hh"
#include "rewrite-derivation.hh"

namespace nix {

using namespace std;

typedef std::map<Path, Path> PathMap;


string replaceSubString(string s, const string toReplace, const string replacement)
{
    size_t j = 0;
    while ((j = s.find(toReplace, j)) != string::npos) {
        debug(format("Found an occurence of %1% in %2%, replacing") % toReplace % s);
        s.replace(j, toReplace.size(), replacement);
    }
    return s;
}

std::string rewriteStrings(std::string s, const StringRewrites & rewrites)
{
    for (auto & i : rewrites) {
        replaceSubString(s, i.first, i.second);
    }
    return s;
}


void removeInputDrv(Derivation* drv, const Path inputDrvPath, const string output)
{
    auto declaredInputOutputs = drv->inputDrvs.find(inputDrvPath);
    if (declaredInputOutputs != drv->inputDrvs.end()) {
        declaredInputOutputs->second.erase(output);
        if (declaredInputOutputs->second.empty()) {
            drv->inputDrvs.erase(declaredInputOutputs);
        }
    }
}

void addInputSrc(Derivation* drv, const Path newInputSrc) {
    drv->inputSrcs.insert(newInputSrc);
}

void recomputeOutputPaths(LocalStore* store, Derivation* drv) {
    for (auto & output: drv->outputs) {
        // XXX: This should only be set if `structuredAttrs` is false
        drv->env[output.first] = "";
        output.second = DerivationOutput("", "", "");
    }

    Hash h = hashDerivationModulo(*store, *drv);

    for (auto & i : drv->outputs) {
        if (i.second.path == "") {
            Path outPath = store->makeOutputPath(i.first, h, "simple-content-addressed");
            drv->env[i.first] = outPath;
            i.second.path = outPath;
            debug(format("Rewrote the path %1% as %2%") % i.first % outPath);
        }
    }

    debug(drv->unparse());

}


bool replacePathIfAlias(Derivation * drv, LocalStore* store, const Path inputDrvPath, Derivation inputDrv, const string outputName)
{
    auto inputPath = inputDrv.outputs[outputName].path;
    auto inputPathInfo = store->queryPathInfo(inputPath);
    auto aliasTo = inputPathInfo->aliasTo;
    if (!aliasTo.empty()) {
        debug(format("Replacing alias %1% by its target %2%") % inputPath % aliasTo);

        removeInputDrv(drv, inputDrvPath, outputName);
        addInputSrc(drv, aliasTo);

        std::map<std::string, std::string> newEnv;
        for (auto envVar : drv->env) {
          auto newVar = replaceSubString(envVar.second, inputPath, aliasTo);
          newEnv[envVar.first] = newVar;
          debug(format("%1% = %2%") % envVar.first % newVar);
        }
        drv->env = newEnv;

        for (auto arg: drv->args)
          replaceSubString(arg, inputPath, aliasTo);

        replaceSubString(drv->builder, inputPath, aliasTo);

        return true;
    } else {
      return false;
    }
}


/**
 * Modify the given drv in-place to replace every reference to an alias path by
 * the path it's supposed to point to.
 *
 * Returns true iff the drv has been modified
**/
bool rebuildDrvForCasInputs(LocalStore* store, BasicDerivation* drv)
{
    auto real_drv = dynamic_cast<Derivation *>(drv);
    bool hasBeenModified = false;
    PathMap aliasesPaths;
      auto inputDrvs = real_drv->inputDrvs;
      for (auto & i : inputDrvs) {
        /* Add the relevant output closures of the input derivation
        `i' as input paths.  Only add the closures of output paths
        that are specified as inputs. */
        assert(store->isValidPath(i.first));
        Derivation inDrv = store->derivationFromPath(i.first);
        for (auto& j : i.second) {
            if (inDrv.outputs.find(j) != inDrv.outputs.end()) {
                bool replacedSomething = replacePathIfAlias(real_drv, store, i.first, inDrv, j);
                hasBeenModified = hasBeenModified || replacedSomething;
            }
        }
    }

    recomputeOutputPaths(store, real_drv);

    return hasBeenModified;
}

} // namespace nix
