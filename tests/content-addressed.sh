source common.sh

clearStore
clearCache

outPath=$(nix-build ./content-addressed.nix)

# Wait one sec to be sure that the `ca` derivation in `content-addressed.nix` has changed
# thanks to the use of `builtins.currentTime`
sleep 1
# Check that although `ca` has been modified we don't build its dependencies
# again because its output remains the same
nix-build ./content-addressed.nix |& (if grep -q "building dep"; then false; else true; fi)

# Make sure that the registration was correct
nix-store --verify --check-contents

# Try copying back and forth from the store
nix copy --to file://$cacheDir $outPath
clearStore
nix copy --no-check-sigs --from file://$cacheDir $outPath
# There's no test for that because I don't know how to do that, but at that
# point we should have the aliases correctly restored
