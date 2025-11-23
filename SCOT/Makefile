MYDEPS = \
	HarrisLinkedListNR.hpp \
	HarrisLinkedListEBR.hpp \
	HarrisLinkedListLFHP.hpp \
	HarrisLinkedListLFHPO.hpp \
	HarrisLinkedListLFHE.hpp \
	HarrisLinkedListLFIBR.hpp \
	HarrisLinkedListLFHyaline.hpp \
	HarrisLinkedListWFHP.hpp \
	HarrisLinkedListWFHPO.hpp \
	HarrisLinkedListWFHE.hpp \
	HarrisLinkedListWFIBR.hpp \
	HarrisLinkedListWFHyaline.hpp \
	HarrisMichaelLinkedListNR.hpp \
	HarrisMichaelLinkedListHP.hpp \
	HarrisMichaelLinkedListHPO.hpp \
	HarrisMichaelLinkedListHE.hpp \
	HarrisMichaelLinkedListEBR.hpp \
	HarrisMichaelLinkedListIBR.hpp \
	HarrisMichaelLinkedListHyaline.hpp \
	NatarajanMittalTreeNR.hpp \
	NatarajanMittalTreeHP.hpp \
	NatarajanMittalTreeHPO.hpp \
	NatarajanMittalTreeHE.hpp \
	NatarajanMittalTreeEBR.hpp \
	NatarajanMittalTreeIBR.hpp \
	NatarajanMittalTreeHyaline.hpp

bench: $(MYDEPS) bench.cpp BenchmarkLists.hpp
	clang++ -O3 -std=c++14 bench.cpp -o bench -lstdc++ -lpthread -lmimalloc

all: bench

clean:
	rm -f bench	
