#!/usr/bin/env bash

# Remove all the arguments before `--`
while [[ $1 != "--" ]]; do
  shift
done
shift

nix copy --to "$REMOTE_STORE" --no-require-sigs "$@"
echo Pushing "$@" to "$REMOTE_STORE"
