libs = libexpr libstore libutil

# * `shared-libasan` necessary because of `-Wl,-z,defs` in `mk/libraries.mk`.
#   See https://github.com/google/sanitizers/wiki/AddressSanitizer#faq ;
# * `AddressSanitizerUseAfterScope` seems to raise a lot of false positive;
fuzz_CXXFLAGS = $(nix_CXXFLAGS) \
								-fsanitize=address,fuzzer \
								-shared-libasan \
								-fno-sanitize-address-use-after-scope

ifeq ($(BUILD_DEBUG), 1)
	fuzz_CXXFLAGS += -O0
else
	fuzz_CXXFLAGS += -O1
endif

fuzz_LDFLAGS = $(nix_LDFLAGS) $(foreach l,$(libs),-Lsrc/$(l) $(subst lib,nix,-l$(l)))

fuzzer: fuzz/fuzz_target.cc $(foreach l,$(libs),src/$(l)/$(subst lib,libnix,$(l)).$(SO_EXT))
	$(CXX) $(GLOBAL_CXXFLAGS) $(fuzz_CXXFLAGS) $(fuzz_LDFLAGS) $(BDW_GC_LIBS) $< -o fuzzer


clean-files += fuzz/*.o fuzzer
