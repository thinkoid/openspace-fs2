# Standalone SEXP parse oracle. Deliberately NOT part of the meson game build:
# it links only sexp_reader.o + main.o -- NO game object files, NO parselo, NO
# ship/object/weapon. That it builds and runs at all is the carve proof.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra

sexp_parse: sexp_reader.o main.o
	$(CXX) $(CXXFLAGS) -o $@ $^

sexp_reader.o: sexp_reader.cpp sexp_reader.h sexp_ops.inc
main.o:        main.cpp sexp_reader.h

# parse the whole retail campaign corpus (loose .fs2 files, no cfile needed)
CORPUS := ../../../missions
check: sexp_parse
	./sexp_parse $(CORPUS)/*.fs2

clean:
	rm -f sexp_parse sexp_reader.o main.o

.PHONY: check clean
