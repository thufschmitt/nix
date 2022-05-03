#include "command.hh"
#include "progress-bar.hh"

using namespace nix;

struct CmdRandomPath : StoreCommand
{
    void run(ref<Store> store) override {
        auto randomPath = store->randomStorePath();
        stopProgressBar();
        logger->writeToStdout(store->printStorePath(randomPath));
    }

    Category category() override {
        return catSecondary;
    }

    std::string description() override {
        return "return a random valid store path from the underlying store";
    }

    std::string doc() override {
        return R""(
        Return a random valid store path from the underlying store.

        **Don’t use**, this is just a silly thing written for the sake of showing the codebase.
                )"";
    }
};

static auto rCmdRandomPath = registerCommand2<CmdRandomPath>({"store", "random-path"});
