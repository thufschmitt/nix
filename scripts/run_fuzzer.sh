#!/usr/bin/env bash

ASAN_OPTIONS="detect_leaks=false" ./fuzzer fuzz/corpus/ fuzz/seeds/
