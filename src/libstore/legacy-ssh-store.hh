#pragma once

#include "callback.hh"
#include "pool.hh"
#include "store-api.hh"
#include "ssh.hh"

namespace nix {
struct LegacySSHStoreConfig : virtual StoreConfig
{
    using StoreConfig::StoreConfig;
    const Setting<int> maxConnections{(StoreConfig*) this, 1, "max-connections", "maximum number of concurrent SSH connections"};
    const Setting<Path> sshKey{(StoreConfig*) this, "", "ssh-key", "path to an SSH private key"};
    const Setting<std::string> sshPublicHostKey{(StoreConfig*) this, "", "base64-ssh-public-host-key", "The public half of the host's SSH key"};
    const Setting<bool> compress{(StoreConfig*) this, false, "compress", "whether to compress the connection"};
    const Setting<Path> remoteProgram{(StoreConfig*) this, "nix-store", "remote-program", "path to the nix-store executable on the remote system"};
    const Setting<std::string> remoteStore{(StoreConfig*) this, "", "remote-store", "URI of the store on the remote system"};

    const std::string name() override { return "Legacy SSH Store"; }
};

struct LegacySSHStore : public virtual LegacySSHStoreConfig, public virtual Store {
    // Hack for getting remote build log output.
    // Intentionally not in `LegacySSHStoreConfig` so that it doesn't appear in
    // the documentation
    const Setting<int> logFD{(StoreConfig*) this, -1, "log-fd", "file descriptor to which SSH's stderr is connected"};

    struct Connection
    {
        std::unique_ptr<SSHMaster::Connection> sshConn;
        FdSink to;
        FdSource from;
        int remoteVersion;
        bool good = true;
    };

    std::string host;

    ref<Pool<Connection>> connections;

    SSHMaster master;

    static std::set<std::string> uriSchemes() { return {"ssh"}; }

    std::string getUri() override
    {
        return *uriSchemes().begin() + "://" + host;
    }

    LegacySSHStore(const std::string & scheme, const std::string & host, const Params & params);

    ref<Connection> openConnection();

    void queryPathInfoUncached(const StorePath & path,
        Callback<std::shared_ptr<const ValidPathInfo>> callback) noexcept override;
    void addToStore(const ValidPathInfo & info, Source & source,
        RepairFlag repair, CheckSigsFlag checkSigs) override;
    void narFromPath(const StorePath & path, Sink & sink) override;
    std::optional<StorePath> queryPathFromHashPart(const std::string & hashPart) override
    { unsupported("queryPathFromHashPart"); }

    StorePath addToStore(
        std::string_view name,
        const Path & srcPath,
        FileIngestionMethod method,
        HashType hashAlgo,
        PathFilter & filter,
        RepairFlag repair,
        const StorePathSet & references) override
    { unsupported("addToStore"); }

    StorePath addTextToStore(
        std::string_view name,
        std::string_view s,
        const StorePathSet & references,
        RepairFlag repair) override
    { unsupported("addTextToStore"); }

    BuildResult buildDerivation(const StorePath & drvPath, const BasicDerivation & drv,
        BuildMode buildMode) override;

    void buildPaths(const std::vector<DerivedPath> & drvPaths, BuildMode buildMode, std::shared_ptr<Store> evalStore) override;

    void computeFSClosure(const StorePathSet & paths,
        StorePathSet & out, bool flipDirection = false,
        bool includeOutputs = false, bool includeDerivers = false) override;

    void ensurePath(const StorePath & path) override
    { unsupported("ensurePath"); }

    StorePathSet queryValidPaths(const StorePathSet & paths,
        SubstituteFlag maybeSubstitute = NoSubstitute) override;

    void connect() override
    {
        auto conn(connections->get());
    }

    unsigned int getProtocol() override
    {
        auto conn(connections->get());
        return conn->remoteVersion;
    }

    void queryRealisationUncached(const DrvOutput &,
        Callback<std::shared_ptr<const Realisation>> callback) noexcept override
    // TODO: Implement
    { unsupported("queryRealisation"); }

private:
    void putBuildSettings(Connection & conn);
};
}
