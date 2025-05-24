MYDEPS = \
	HarrisLinkedListNR.hpp \
	HarrisLinkedListHP.hpp \
	HarrisLinkedListHE.hpp \
	HarrisLinkedListEBR.hpp \
	HarrisLinkedListIBR.hpp \
	HarrisLinkedListHyaline.hpp \
	HarrisLinkedListRecHP.hpp \
	HarrisLinkedListRecHE.hpp \
	HarrisLinkedListRecIBR.hpp \
	HarrisLinkedListRecHyaline.hpp \
	HarrisMichaelLinkedListNR.hpp \
	HarrisMichaelLinkedListHP.hpp \
	HarrisMichaelLinkedListHE.hpp \
	HarrisMichaelLinkedListEBR.hpp \
	HarrisMichaelLinkedListIBR.hpp \
	HarrisMichaelLinkedListHyaline.hpp \
	NatarajanMittalTreeNR.hpp \
	NatarajanMittalTreeHP.hpp \
	NatarajanMittalTreeHE.hpp \
	NatarajanMittalTreeEBR.hpp \
	NatarajanMittalTreeIBR.hpp \
	NatarajanMittalTreeHyaline.hpp

bench: $(MYDEPS) bench.cpp BenchmarkLists.hpp
	clang++ -O3 -std=c++14 bench.cpp -o bench -lstdc++ -lpthread -lmimalloc

all: bench

clean:
	rm -f bench	
