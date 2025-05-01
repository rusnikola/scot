MYDEPS = \
	HarrisLinkedListNR.hpp \
	HarrisLinkedListHP.hpp \
	HarrisLinkedListEBR.hpp \
	HarrisLinkedListIBR.hpp \
	HarrisMichaelLinkedListNR.hpp \
	HarrisMichaelLinkedListHP.hpp \
	HarrisMichaelLinkedListEBR.hpp \
	HarrisMichaelLinkedListIBR.hpp \
	NatarajanMittalTreeNR.hpp \
	NatarajanMittalTreeHP.hpp \
	NatarajanMittalTreeEBR.hpp \
	NatarajanMittalTreeIBR.hpp

bench: $(MYDEPS) bench.cpp BenchmarkLists.hpp
	clang++ -O3 -std=c++14 bench.cpp -o bench -lstdc++ -lpthread -ljemalloc

all: bench

clean:
	rm -f bench	
