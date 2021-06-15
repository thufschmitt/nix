libs = libexpr libstore libutil

fuzz_CXXFLAGS = $(nix_CXXFLAGS) -O1 -fsanitize=fuzzer
fuzz_LDFLAGS = $(nix_LDFLAGS) $(foreach l,$(libs),-Lsrc/$(l) $(subst lib,nix,-l$(l)))

fuzzer: fuzz/fuzz_target.cc $(foreach l,$(libs),src/$(l)/$(subst lib,libnix,$(l)).$(SO_EXT))
	$(CXX) $(GLOBAL_CXXFLAGS) $(fuzz_CXXFLAGS) $(fuzz_LDFLAGS) $(BDW_GC_LIBS) $< -o fuzzer


clean-files += fuzz/*.o fuzzer
