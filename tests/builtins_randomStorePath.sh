source common.sh

requireDaemonNewerThan 20220525

cd "$TEST_ROOT"

for i in {0..100}; do
    echo "$i" > foo
    nix-store --add foo
done

[[ $(nix-instantiate --eval --expr 'builtins.randomStorePath 0') =~ "$NIX_STORE_DIR" ]]

for i in {0..10}; do
    nix-instantiate --eval --expr 'builtins.storePath (builtins.randomStorePath 1)'
done
