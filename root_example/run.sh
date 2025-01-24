#!/bin/sh

./@cgn.d/build_linux/cgn --halt_on_error --target llvm,debug,asan run $@