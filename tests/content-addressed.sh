source common.sh

clearStore

startDaemon

# cgdb -- --init-command gdbinit --args nix-build -j1 -vvvvv ./content-addressed.nix
nix-build -j1 -vvvvv ./content-addressed.nix

