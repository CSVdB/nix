#pragma once

#include "args.hh"
#include "common-eval-args.hh"
#include <optional>

namespace nix {

extern std::string programPath;

struct Value;
class Bindings;
class EvalState;
class Store;

namespace flake {
enum HandleLockFile : unsigned int;
}

/* A command that require a Nix store. */
struct StoreCommand : virtual Command
{
    StoreCommand();
    void run() override;
    ref<Store> getStore();
    virtual ref<Store> createStore();
    virtual void run(ref<Store>) = 0;

private:
    std::shared_ptr<Store> _store;
};

struct Buildable
{
    Path drvPath; // may be empty
    std::map<std::string, Path> outputs;
};

typedef std::vector<Buildable> Buildables;

struct App
{
    PathSet context;
    Path program;
    // FIXME: add args, sandbox settings, metadata, ...

    App(EvalState & state, Value & vApp);
};

struct Installable
{
    virtual std::string what() = 0;

    virtual Buildables toBuildables()
    {
        throw Error("argument '%s' cannot be built", what());
    }

    Buildable toBuildable();

    App toApp(EvalState & state);

    virtual Value * toValue(EvalState & state)
    {
        throw Error("argument '%s' cannot be evaluated", what());
    }
};

struct EvalCommand : virtual StoreCommand, MixEvalArgs
{
    ref<EvalState> getEvalState();

    ~EvalCommand();

private:

    std::shared_ptr<EvalState> evalState;
};

struct MixFlakeOptions : virtual Args
{
    bool recreateLockFile = false;

    bool saveLockFile = true;

    bool useRegistries = true;

    MixFlakeOptions();

    flake::HandleLockFile getLockFileMode();
};

struct SourceExprCommand : virtual Args, EvalCommand, MixFlakeOptions
{
    std::optional<Path> file;

    SourceExprCommand();

    std::vector<std::shared_ptr<Installable>> parseInstallables(
        ref<Store> store, std::vector<std::string> ss);

    std::shared_ptr<Installable> parseInstallable(
        ref<Store> store, const std::string & installable);

    virtual Strings getDefaultFlakeAttrPaths()
    {
        return {"defaultPackage"};
    }

    virtual Strings getDefaultFlakeAttrPathPrefixes()
    {
        return {
            // As a convenience, look for the attribute in
            // 'outputs.packages'.
            "packages.",
            // As a temporary hack until Nixpkgs is properly converted
            // to provide a clean 'packages' set, look in 'legacyPackages'.
            "legacyPackages."
        };
    }
};

enum RealiseMode { Build, NoBuild, DryRun };

/* A command that operates on a list of "installables", which can be
   store paths, attribute paths, Nix expressions, etc. */
struct InstallablesCommand : virtual Args, SourceExprCommand
{
    std::vector<std::shared_ptr<Installable>> installables;

    InstallablesCommand()
    {
        expectArgs("installables", &_installables);
    }

    void prepare() override;

    virtual bool useDefaultInstallables() { return true; }

private:

    std::vector<std::string> _installables;
};

struct InstallableCommand : virtual Args, SourceExprCommand
{
    std::shared_ptr<Installable> installable;

    InstallableCommand()
    {
        expectArg("installable", &_installable, true);
    }

    void prepare() override;

private:

    std::string _installable{"."};
};

/* A command that operates on zero or more store paths. */
struct StorePathsCommand : public InstallablesCommand
{
private:

    bool recursive = false;
    bool all = false;

public:

    StorePathsCommand(bool recursive = false);

    using StoreCommand::run;

    virtual void run(ref<Store> store, Paths storePaths) = 0;

    void run(ref<Store> store) override;

    bool useDefaultInstallables() override { return !all; }
};

/* A command that operates on exactly one store path. */
struct StorePathCommand : public InstallablesCommand
{
    using StoreCommand::run;

    virtual void run(ref<Store> store, const Path & storePath) = 0;

    void run(ref<Store> store) override;
};

/* A helper class for registering commands globally. */
struct RegisterCommand
{
    static Commands * commands;

    RegisterCommand(const std::string & name,
        std::function<ref<Command>()> command)
    {
        if (!commands) commands = new Commands;
        commands->emplace(name, command);
    }
};

template<class T>
static RegisterCommand registerCommand(const std::string & name)
{
    return RegisterCommand(name, [](){ return make_ref<T>(); });
}

Buildables build(ref<Store> store, RealiseMode mode,
    std::vector<std::shared_ptr<Installable>> installables);

PathSet toStorePaths(ref<Store> store, RealiseMode mode,
    std::vector<std::shared_ptr<Installable>> installables);

Path toStorePath(ref<Store> store, RealiseMode mode,
    std::shared_ptr<Installable> installable);

PathSet toDerivations(ref<Store> store,
    std::vector<std::shared_ptr<Installable>> installables,
    bool useDeriver = false);

}
