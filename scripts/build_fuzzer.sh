#!/usr/bin/env bash

export BUILD_DEBUG=1

./bootstrap.sh
./configure $configureFlags --prefix="$(pwd)/outputs/out"

# * `shared-libasan` necessary because of `-Wl,-z,defs` in `mk/libraries.mk`.
#   See https://github.com/google/sanitizers/wiki/AddressSanitizer#faq ;
# * `AddressSanitizerUseAfterScope` seems to raise a lot of false positive;
make fuzzer \
  CXXFLAGS="-O0 -fsanitize=address -fno-sanitize-address-use-after-scope" \
  LDFLAGS="-fsanitize=address -shared-libasan" \
  OPTIMIZE=0
