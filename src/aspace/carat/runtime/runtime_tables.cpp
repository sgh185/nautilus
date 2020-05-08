
//This header file holds the functions that will be injected into any program using Carat. The functions are responsible for building the runtime tables
//as well as holding the PDG (will have to figure this out).

//Determine C vs C++ (Probably C++)


#include "runtime_tables.hpp"



//Alloc addr, length
std::map<void*, allocEntry*>* allocationMap = nullptr;

allocEntry* StackEntry;
uint64_t rsp = 0;

uint64_t escapeWindowSize = 0;
uint64_t totalEscapeEntries = 0;
void*** escapeWindow = nullptr;


class texasStartup{
	public:
		texasStartup(){
			user_init();
			texas_init();
		}
} __texasStartup;


allocEntry::allocEntry(void* ptr, uint64_t len, std::string varName, std::string fileOri, uint64_t lineNum, uint64_t colNum){
    pointer = ptr;
    length = len;
    variableName = varName;
    origin.push_back(std::make_tuple(fileOri, lineNum, colNum));
}

allocEntry::allocEntry(void* ptr, uint64_t len){
    pointer = ptr;
    length = len;
}

//This can be built to find all the pointer connections of a program
std::map<allocEntry*, std::map<allocEntry*, uint64_t>*> allocConnections;


int64_t doesItAlias(void* allocAddr, uint64_t length, uint64_t escapeVal){
	uint64_t blockStart = (uint64_t) allocAddr;
	if(escapeVal >= blockStart && escapeVal < (blockStart + length)){
		return escapeVal - blockStart;
	}
	else{
		return -1;
	}
}

void AddToAllocationTable(void* address, uint64_t length){
	//printf("In add to alloc: %p, %lu, %p\n", address, length, allocationMap);
	//fflush(stdout);
	allocEntry* newEntry = new allocEntry(address, length);
	//printf("Adding to allocationMap\n");
	//fflush(stdout);
	allocationMap->insert({address, newEntry});
	//printf("Returning\n");
	//fflush(stdout);
	return;
}

void AddCallocToAllocationTable(void* address, uint64_t len, uint64_t sizeOfEntry){
	uint64_t length = len * sizeOfEntry;
	auto newEntry = new allocEntry(address, length);
	allocationMap->insert_or_assign(address, newEntry);
	return;
}

void HandleReallocInAllocationTable(void* address, void* newAddress, uint64_t length){
	allocationMap->erase(address);
	auto newEntry = new allocEntry(address, length);
	allocationMap->insert_or_assign(address, newEntry);
	return;
}



/*
 * 1) Search for address escaping within allocation table (an ordered map that contains all current allocations <non overlapping blocks of memory each consisting of an  address and length> made by the program
 * 2) If found (the addressEscaping variable falls within one of the blocks), then a new entry into the escape table is made consisting of the addressEscapingTo, the addressEscaping, and the length of the addressEscaping (for optimzation if consecutive escapes occur)
 */
void AddToEscapeTable(void* addressEscaping){
	if(totalEscapeEntries >= escapeWindowSize){
		processEscapeWindow();
	}
	escapeWindow[totalEscapeEntries] = (void**)addressEscaping;
	totalEscapeEntries++;
	//fprintf(stderr, "CARAT: %016lx\n", __builtin_return_address(0));


}

allocEntry* findAllocEntry(void* address){
	auto prospective = allocationMap->lower_bound(address);
	uint64_t blockStart;
	uint64_t blockLen;

	//prospective will be either not found at all, matches addressEscaping exactly, or is next block after addressEscaping      

	//Not found at all
	if(prospective == allocationMap->end()){
		return nullptr;
	}
	blockStart = (uint64_t)prospective->first;
	blockLen = prospective->second->length;
	//Matches exactly
	if(blockStart == (uint64_t)address){
		//Nothing to do, prospective points to our matching block
	}
	//Could be in previous block
	else if (prospective != allocationMap->begin()){
		prospective--;
		blockStart = (uint64_t)prospective->first;
		blockLen = prospective->second->length;
		if(doesItAlias(prospective->first, blockLen, (uint64_t)address) == -1){
			//Not found
			return nullptr;
		}
	}
	else{
		return nullptr;
	}

	return prospective->second;

}

void processEscapeWindow(){
	//printf("\n\n\nI am invoked!\n\n\n");
	fflush(stdout);
	std::unordered_set<void**> processedEscapes;
	for (uint64_t i = 0; i < totalEscapeEntries; i++){
		void** addressEscaping = escapeWindow[i];

		if(processedEscapes.find(addressEscaping) != processedEscapes.end()){
			continue;
		}
		processedEscapes.insert(addressEscaping);
 		if(addressEscaping == NULL){
 			continue;
 		}
		auto* alloc = findAllocEntry(*addressEscaping);

		if(alloc == nullptr){
			continue;
		}

		//We have found block
		//Inserting the object address escaping along with the length it is running for
		// and the length (usually 1) into escape table with key of addressEscapingTo
		//One can reverse engineer the starting point if the length is longer than 1
		//by subtracting from value in dereferenced addressEscapingTo
		alloc->allocToEscapeMap.insert(addressEscaping);  
	}
	totalEscapeEntries = 0;

}


//This function will remove an address from the allocation from a free() or free()-like instruction being called
void RemoveFromAllocationTable(void* address){
	allocationMap->erase(address);
}

void GenerateConnectionGraph(){
	//first kill off the old state allocConnections
  std::set<void**> doneEscapes;
  allocConnections.clear();
  //Initialize
  for(auto allocs : *allocationMap){
    std::map<allocEntry*, uint64_t>* newConnection = new std::map<allocEntry*, uint64_t>();
    allocConnections.insert_or_assign(allocs.second, newConnection);
    allocs.second->totalPointerWeight = 0;
  }

	//Iterate through all the allocations of the allocationMap
	for(auto entry : *allocationMap){
		auto alloc = entry.second;
		//Look at each escape and determine if it falls in another alloc
		for(void** candidateEscape : alloc->allocToEscapeMap){
      //No repetitive work across allocations
      if(doneEscapes.find(candidateEscape) != doneEscapes.end()){
        continue;
      }
      doneEscapes.insert(candidateEscape);

			//First verify that it still points to alloc we are dealing with
			if(doesItAlias(alloc->pointer, alloc->length, (uint64_t)(*candidateEscape) ) == -1){
				continue;
			}
			//Now see if the allocation lives in one of the allocationMap entries
			allocEntry* pointerAlloc = findAllocEntry((void*)candidateEscape);
			if(pointerAlloc == nullptr){
				continue;
			}
			//We now know that pointerAlloc contains a pointer that points to alloc
      //Now we must modify that allocations allocConnections entry (it also must exist)
			auto connectionEntry = allocConnections.find(pointerAlloc);
      uint64_t newVal = 1;
      //Find the entry to alloc in the pointerAlloc's connection graph
      auto allocInConnectionEntry = connectionEntry->second->find(alloc);
      if(allocInConnectionEntry != connectionEntry->second->end()){
        newVal += allocInConnectionEntry->second;
      }
      connectionEntry->second->insert_or_assign(alloc, newVal);
      alloc->totalPointerWeight++;		
    }
	}
}


void ReportStatistics(){
	printf("Size of Allocation Table: %lu\n", (uint64_t)allocationMap->size());  
}


uint64_t getrsp(){
	uint64_t retVal;
	__asm__ __volatile__("movq %%rsp, %0" : "=a"(retVal) : : "memory");
	return retVal;
}


void texas_init(){
	rsp = getrsp();
	allocationMap = new std::map<void*, allocEntry*>();
	escapeWindowSize = 1048576;
	totalEscapeEntries = 0;
	//This adds the stack pointer to a tracked alloc that is length of 32GB which encompasses all programs we run
	void* rspVoidPtr = (void*)(rsp-0x800000000ULL);
	StackEntry = new allocEntry(rspVoidPtr, 0x800000000ULL);
	allocationMap->insert_or_assign(rspVoidPtr, StackEntry);
	escapeWindow = (void***)calloc(escapeWindowSize, sizeof(void*));

	//printf("Leaving texas_init\n");
	return;
}