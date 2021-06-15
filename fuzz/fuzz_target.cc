#include "attr-path.hh"
#include "eval.hh"
#include "get-drvs.hh"
#include "globals.hh"
#include "local-fs-store.hh"
#include "store-api.hh"
#include "types.hh"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {

  using namespace nix;

  // Initialize some stuff;
  // See `src/nix-instantiate/nix-instantiate.cc` for more info.

  initGC();
  static Path gcRoot = "";
  static int rootNr = 0;


  // TODO: real opened store VS dummy?
  // auto store = openStore();
  // auto state = std::make_unique<EvalState>(Strings(), store);
  auto state = std::make_unique<EvalState>(Strings(), openStore("dummy://"));
  state->repair = Repair;

  Strings attrPaths = {""};
  Bindings * autoArgs = state->allocBindings(0);

  // Format input data into an expression

  const char * expression_string = std::string(reinterpret_cast<const char*>(Data), Size).c_str();
  Expr * expression;
  try {
    expression = state->parseExprFromString(expression_string, absPath("."));
  } catch (const nix::ParseError & e) {
    // Parse errors are legitimate, so we want to gracefully return here.
    return 0;
  }

  // Parse and evaluate the expression, update the store representation;
  // Adapted from `processExpr` in `src/nix-instantiate/nix-instantiate.cc`.

  Value vRoot;
  state->eval(expression, vRoot);

  for (auto & i : attrPaths) {
      Value & v(*findAlongAttrPath(*state, i, *autoArgs, vRoot).first);
      state->forceValue(v);

      PathSet context;
      DrvInfos drvs;
      getDerivations(*state, v, "", *autoArgs, drvs, false);
      for (auto & i : drvs) {
          Path drvPath = i.queryDrvPath();

          string outputName = i.queryOutputName();
          if (outputName == "")
              // Real code throws an error in that case, as it lacks the `outputName` attribute;
              // We exit gracefully instead.
              return 0;
          else {
              Path rootName = absPath(gcRoot);
              if (++rootNr > 1) rootName += "-" + std::to_string(rootNr);
              auto store2 = state->store.dynamic_pointer_cast<LocalFSStore>();
              if (store2)
                  drvPath = store2->addPermRoot(store2->parseStorePath(drvPath), rootName);
          }
      }
  }

  return 0;
}
