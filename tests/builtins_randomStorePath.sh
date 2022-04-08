source common.sh

nix-instantiate --eval --expr 'builtins.randomStorePath'
