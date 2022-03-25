#include "archive.hh"
#include "remote-store.hh"
#include "serve-protocol.hh"
#include "build-result.hh"
#include "store-api.hh"
#include "path-with-outputs.hh"
#include "worker-protocol.hh"
#include "derivations.hh"
#include "callback.hh"
#include "legacy-ssh-store.hh"

namespace nix {

LegacySSHStore::LegacySSHStore(const std::string & scheme, const std::string & host, const Params & params)
        : StoreConfig(params)
        , LegacySSHStoreConfig(params)
        , Store(params)
        , host(host)
        , connections(make_ref<Pool<Connection>>(
            std::max(1, (int) maxConnections),
            [this]() { return openConnection(); },
            [](const ref<Connection> & r) { return r->good; }
            ))
        , master(
            host,
            sshKey,
            sshPublicHostKey,
            // Use SSH master only if using more than 1 connection.
            connections->capacity() > 1,
            compress,
            logFD)
{
}

ref<LegacySSHStore::Connection> LegacySSHStore::openConnection()
{
    auto conn = make_ref<Connection>();
    conn->sshConn = master.startCommand(
        fmt("%s --serve --write", remoteProgram)
        + (remoteStore.get() == "" ? "" : " --store " + shellEscape(remoteStore.get())));
    conn->to = FdSink(conn->sshConn->in.get());
    conn->from = FdSource(conn->sshConn->out.get());

    try {
        conn->to << SERVE_MAGIC_1 << SERVE_PROTOCOL_VERSION;
        conn->to.flush();

        StringSink saved;
        try {
            TeeSource tee(conn->from, saved);
            unsigned int magic = readInt(tee);
            if (magic != SERVE_MAGIC_2)
                throw Error("'nix-store --serve' protocol mismatch from '%s'", host);
        } catch (SerialisationError & e) {
            /* In case the other side is waiting for our input,
                close it. */
            conn->sshConn->in.close();
            auto msg = conn->from.drain();
            throw Error("'nix-store --serve' protocol mismatch from '%s', got '%s'",
                host, chomp(saved.s + msg));
        }
        conn->remoteVersion = readInt(conn->from);
        if (GET_PROTOCOL_MAJOR(conn->remoteVersion) != 0x200)
            throw Error("unsupported 'nix-store --serve' protocol version on '%s'", host);

    } catch (EndOfFile & e) {
        throw Error("cannot connect to '%1%'", host);
    }

    return conn;
};

void LegacySSHStore::queryPathInfoUncached(const StorePath & path,
        Callback<std::shared_ptr<const ValidPathInfo>> callback) noexcept
{
    try {
        auto conn(connections->get());

        /* No longer support missing NAR hash */
        assert(GET_PROTOCOL_MINOR(conn->remoteVersion) >= 4);

        debug("querying remote host '%s' for info on '%s'", host, printStorePath(path));

        conn->to << cmdQueryPathInfos << PathSet{printStorePath(path)};
        conn->to.flush();

        auto p = readString(conn->from);
        if (p.empty()) return callback(nullptr);
        auto path2 = parseStorePath(p);
        assert(path == path2);
        /* Hash will be set below. FIXME construct ValidPathInfo at end. */
        auto info = std::make_shared<ValidPathInfo>(path, Hash::dummy);

        PathSet references;
        auto deriver = readString(conn->from);
        if (deriver != "")
            info->deriver = parseStorePath(deriver);
        info->references = worker_proto::read(*this, conn->from, Phantom<StorePathSet> {});
        readLongLong(conn->from); // download size
        info->narSize = readLongLong(conn->from);

        {
            auto s = readString(conn->from);
            if (s == "")
                throw Error("NAR hash is now mandatory");
            info->narHash = Hash::parseAnyPrefixed(s);
        }
        info->ca = parseContentAddressOpt(readString(conn->from));
        info->sigs = readStrings<StringSet>(conn->from);

        auto s = readString(conn->from);
        assert(s == "");

        callback(std::move(info));
    } catch (...) { callback.rethrow(); }
}

void LegacySSHStore::addToStore(const ValidPathInfo & info, Source & source,
    RepairFlag repair, CheckSigsFlag checkSigs)
{
    debug("adding path '%s' to remote host '%s'", printStorePath(info.path), host);

    auto conn(connections->get());

    if (GET_PROTOCOL_MINOR(conn->remoteVersion) >= 5) {

        conn->to
            << cmdAddToStoreNar
            << printStorePath(info.path)
            << (info.deriver ? printStorePath(*info.deriver) : "")
            << info.narHash.to_string(Base16, false);
        worker_proto::write(*this, conn->to, info.references);
        conn->to
            << info.registrationTime
            << info.narSize
            << info.ultimate
            << info.sigs
            << renderContentAddress(info.ca);
        try {
            copyNAR(source, conn->to);
        } catch (...) {
            conn->good = false;
            throw;
        }
        conn->to.flush();

    } else {

        conn->to
            << cmdImportPaths
            << 1;
        try {
            copyNAR(source, conn->to);
        } catch (...) {
            conn->good = false;
            throw;
        }
        conn->to
            << exportMagic
            << printStorePath(info.path);
        worker_proto::write(*this, conn->to, info.references);
        conn->to
            << (info.deriver ? printStorePath(*info.deriver) : "")
            << 0
            << 0;
        conn->to.flush();

    }

    if (readInt(conn->from) != 1)
        throw Error("failed to add path '%s' to remote host '%s'", printStorePath(info.path), host);
}

void LegacySSHStore::narFromPath(const StorePath & path, Sink & sink)
{
    auto conn(connections->get());

    conn->to << cmdDumpStorePath << printStorePath(path);
    conn->to.flush();
    copyNAR(conn->from, sink);
}

void LegacySSHStore::putBuildSettings(Connection & conn)
{
    conn.to
        << settings.maxSilentTime
        << settings.buildTimeout;
    if (GET_PROTOCOL_MINOR(conn.remoteVersion) >= 2)
        conn.to
            << settings.maxLogSize;
    if (GET_PROTOCOL_MINOR(conn.remoteVersion) >= 3)
        conn.to
            << settings.buildRepeat
            << settings.enforceDeterminism;

    if (GET_PROTOCOL_MINOR(conn.remoteVersion) >= 7) {
        conn.to << ((int) settings.keepFailed);
    }
}

BuildResult LegacySSHStore::buildDerivation(const StorePath & drvPath, const BasicDerivation & drv,
    BuildMode buildMode)
{
    auto conn(connections->get());

    conn->to
        << cmdBuildDerivation
        << printStorePath(drvPath);
    writeDerivation(conn->to, *this, drv);

    putBuildSettings(*conn);

    conn->to.flush();

    BuildResult status { .path = DerivedPath::Built { .drvPath = drvPath } };
    status.status = (BuildResult::Status) readInt(conn->from);
    conn->from >> status.errorMsg;

    if (GET_PROTOCOL_MINOR(conn->remoteVersion) >= 3)
        conn->from >> status.timesBuilt >> status.isNonDeterministic >> status.startTime >> status.stopTime;
    if (GET_PROTOCOL_MINOR(conn->remoteVersion) >= 6) {
        status.builtOutputs = worker_proto::read(*this, conn->from, Phantom<DrvOutputs> {});
    }
    return status;
}

void LegacySSHStore::buildPaths(const std::vector<DerivedPath> & drvPaths, BuildMode buildMode, std::shared_ptr<Store> evalStore)
{
    if (evalStore && evalStore.get() != this)
        throw Error("building on an SSH store is incompatible with '--eval-store'");

    auto conn(connections->get());

    conn->to << cmdBuildPaths;
    Strings ss;
    for (auto & p : drvPaths) {
        auto sOrDrvPath = StorePathWithOutputs::tryFromDerivedPath(p);
        std::visit(overloaded {
            [&](const StorePathWithOutputs & s) {
                ss.push_back(s.to_string(*this));
            },
            [&](const StorePath & drvPath) {
                throw Error("wanted to fetch '%s' but the legacy ssh protocol doesn't support merely substituting drv files via the build paths command. It would build them instead. Try using ssh-ng://", printStorePath(drvPath));
            },
        }, sOrDrvPath);
    }
    conn->to << ss;

    putBuildSettings(*conn);

    conn->to.flush();

    BuildResult result { .path = DerivedPath::Opaque { StorePath::dummy } };
    result.status = (BuildResult::Status) readInt(conn->from);

    if (!result.success()) {
        conn->from >> result.errorMsg;
        throw Error(result.status, result.errorMsg);
    }
}

void LegacySSHStore::computeFSClosure(const StorePathSet & paths,
    StorePathSet & out, bool flipDirection,
    bool includeOutputs, bool includeDerivers)
{
    if (flipDirection || includeDerivers) {
        Store::computeFSClosure(paths, out, flipDirection, includeOutputs, includeDerivers);
        return;
    }

    auto conn(connections->get());

    conn->to
        << cmdQueryClosure
        << includeOutputs;
    worker_proto::write(*this, conn->to, paths);
    conn->to.flush();

    for (auto & i : worker_proto::read(*this, conn->from, Phantom<StorePathSet> {}))
        out.insert(i);
}

StorePathSet LegacySSHStore::queryValidPaths(const StorePathSet & paths,
    SubstituteFlag maybeSubstitute)
{
    auto conn(connections->get());

    conn->to
        << cmdQueryValidPaths
        << false // lock
        << maybeSubstitute;
    worker_proto::write(*this, conn->to, paths);
    conn->to.flush();

    return worker_proto::read(*this, conn->from, Phantom<StorePathSet> {});
}

static RegisterStoreImplementation<LegacySSHStore, LegacySSHStoreConfig> regLegacySSHStore;

}
