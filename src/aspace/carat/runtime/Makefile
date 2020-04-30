all: texas.bc 

state: texas.bc

manLoc: manLoc.bc


CXXFLAGS= -O3 -g -fPIC -std=c++17
CFLAGS= -O3 -g -fPIC

texas.bc: user_patcher.c patching.cpp runtime_tables.cpp state_tracking.cpp 
	clang   -emit-llvm $(CFLAGS)   -c user_patcher.c -o user_patching.bc
	clang++ -emit-llvm $(CXXFLAGS) -c patching.cpp -o patching.bc
	clang++ -emit-llvm $(CXXFLAGS) -c runtime_tables.cpp -o texas.bc	 
	llvm-link texas.bc patching.bc user_patching.bc -o texas.bc
	clang++ -emit-llvm $(CXXFLAGS) -c state_tracking.cpp
	llvm-link texas.bc state_tracking.bc -o texas.bc 

manLoc.bc: texas.bc state_optimizer.cpp state_packer.c
	clang -emit-llvm $(CFLAGS) -c state_packer.c
	clang++ -emit-llvm $(CXXFLAGS) -c state_optimizer.cpp
	llvm-link state_packer.bc state_optimizer.bc texas.bc -o manLoc.bc

clean: 
	rm -f *.o *.bc *.ll
