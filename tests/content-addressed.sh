source common.sh

clearStore
clearCache

outPath=$(nix-build ./content-addressed.nix)

# Make sure that the registration was correct
nix-store --verify --check-contents

# Try copying back and forth from the store
nix copy --to file://$cacheDir $outPath
clearStore
nix copy --no-check-sigs --from file://$cacheDir $outPath
# There's no test for that because I don't know how to do that, but at that
# point we should have the aliases correctly restored
