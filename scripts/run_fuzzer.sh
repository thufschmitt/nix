#!/usr/bin/env bash

ASAN_OPTIONS="detect_leaks=false" ./fuzzer -fork=8 fuzz/corpus/ fuzz/seeds/
