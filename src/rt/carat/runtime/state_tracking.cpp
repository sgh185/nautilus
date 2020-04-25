#include "state_tracking.hpp"

uint64_t currentStateID = 0;
state* activeState = nullptr;
std::map<uint64_t, state*> stateMap;

state::state(char* funcName, uint64_t lineNum, uint64_t temporalTracking){
    temporalGraph = temporalTracking;
    allocations = new std::set<allocEntry*>();
    functionName = funcName;
    stateID = currentStateID;
    currentStateID++;
    lineNumber = lineNum;
    stateWindow = (void**)malloc(stateWindowSize * sizeof(void*));
    if(activeState){
	parent = activeState;
	parent->children.insert(this); 
    }
}

state::state(){
    temporalGraph = 0;
    allocations = new std::set<allocEntry*>();
    stateID = currentStateID;
    currentStateID++;
    stateWindow = (void**)calloc(stateWindowSize, sizeof(void*));
    if(activeState){
	parent = activeState;
	parent->children.insert(this); 
    }
}

void state::ProcessTemporalGraph(){
   //Initialize new temporal connections
   for(auto alloc : *allocations){
       if(usesInState.find(alloc) == usesInState.end()){
	   usesInState.insert_or_assign(alloc, 0);
       }
   }

   allocEntry* curVal = nullptr;
   bool firstIter = true;
   uint64_t i = 0;
   //Iterate through the temporal vec and rebuild the temporalConnections graphs
   for(uint64_t i = 0; i < totalStateEntries; i++){

       allocEntry* nextVal = (allocEntry*)stateWindow[i];
       DEBUG("PTG 1.5: (%lu): (curVal: %p)\n", i, curVal);
       DEBUG("PTG 2 (%lu):  %p (Mem: %p) (Var: %c)\n", i, nextVal, nextVal->pointer, nextVal->variableName.c_str());

       uint64_t newVal = 1;
       if (firstIter){
	   curVal = nextVal;
	   newVal += usesInState.find(nextVal)->second;
	   usesInState.insert_or_assign(nextVal, newVal);
	   firstIter = false;
	   continue;
       }

       DEBUG("PTG 3.0\n");

       //Assign nextVal to curVals conections
       //This must exist because the first loop in this function
       auto curValTemporalConnectionsNode = temporalConnections.find(curVal);
       std::map<allocEntry*, uint64_t>* allocConnectionMap = nullptr;
       DEBUG("PTG 3.5\n");
       if(curValTemporalConnectionsNode == temporalConnections.end()){
	   DEBUG("curValTemporalConnectionsNode NOT FOUND\n");
	   allocConnectionMap = new std::map<allocEntry*, uint64_t>();
	   temporalConnections.insert_or_assign(curVal, allocConnectionMap);
	   DEBUG("curValTemporalConnectionsNode Created\n");

       }
       else{
	   allocConnectionMap = curValTemporalConnectionsNode->second;
       }


       auto allocConnection = allocConnectionMap->find(nextVal);

       //By default we are at least making a new entry
       newVal = 1;
       DEBUG("PTG 4\n");

       if(allocConnection != allocConnectionMap->end()){
	   //Found entry, let us increase the weight by 1
	   newVal += allocConnection->second;
       }
       //Time to insert the value back into the mapping
       allocConnectionMap->insert_or_assign(nextVal, newVal);
       curVal = nextVal;

       newVal = 1;
       newVal += usesInState.find(nextVal)->second;
       usesInState.insert_or_assign(nextVal, newVal);
       DEBUG("PTG 5\n");

   }
}

void state::processStateWindow(){
    std::unordered_set<void*> processedEntries;
    for(uint64_t i = 0; i < totalStateEntries; i++){
	void* allocPtr = stateWindow[i];

	if(processedEntries.find(allocPtr) != processedEntries.end()){
	    continue;
	}
	processedEntries.insert(allocPtr);

	auto prospective = allocationMap->lower_bound(allocPtr);
	uint64_t blockStart;
	uint64_t blockLen;

	//prospective will be either not found at all, matches addressEscaping exactly, or is next block after addressEscaping      

	//Not found at all
	if(prospective == allocationMap->end()){
	    continue;
	}
	blockStart = (uint64_t)prospective->first;
	blockLen = prospective->second->length;
	//Matches exactly
	if(blockStart == (uint64_t)allocPtr){
	    //Nothing to do, prospective points to our matching block
	}
	//Could be in previous block
	else if(prospective != allocationMap->begin()){
	    prospective--;
	    blockStart = (uint64_t)prospective->first;
	    blockLen = prospective->second->length;
	    if(doesItAlias(prospective->first, blockLen, (uint64_t)allocPtr) == -1){
		//Not found
		continue;
	    }
	}



	//This is our allocEntry
	allocEntry* entry = prospective->second;
	//printf("We have found block: %p\nInside activeState:%p\n", entry, actState);
	//insert alloc into set
	allocations->insert(entry);
	if(temporalGraph){
	    stateWindow[i] = (void*)entry;
	}

	state* parentState = parent;
	if(parentState != nullptr){
	    parentState->stateWindow[parentState->totalStateEntries] = allocPtr;
	    parentState->totalStateEntries++;
	    if(parentState->totalStateEntries >= parentState->stateWindowSize){
		parentState->processStateWindow();
	    }
	}
    }

    if(temporalGraph){
	ProcessTemporalGraph();
    }
    totalStateEntries = 0; 

}


void StateAlloc(void* address, uint64_t length, std::string variableName, std::string fileOrigin, uint64_t lineNumber, uint64_t columnNumber){
	allocEntry* newEntry = new allocEntry(address, length, variableName, fileOrigin, lineNumber, columnNumber);
	allocationMap->insert_or_assign(address, newEntry); 
	return;
}

void StateCalloc(void* address, uint64_t len, uint64_t sizeOfEntry, std::string variableName, std::string fileOrigin, uint64_t lineNumber, uint64_t columnNumber){
	uint64_t length = len * sizeOfEntry;
	StateAlloc(address, length, variableName, fileOrigin, lineNumber, columnNumber);
	return;
}

void StateRealloc(void* address, void* newAddress, uint64_t length, std::string variableName, std::string fileOrigin, uint64_t lineNumber, uint64_t columnNumber){
	allocationMap->erase(address);
	StateAlloc(address, length, variableName, fileOrigin, lineNumber, columnNumber);
	return;
}

void StateFree(void* address){
	allocationMap->erase(address);
	return;
}


//This will be placed outside of a block of code we are tracking and will return the key of the
//state or it will create a state and assign a new key
uint64_t GetState(char* funcName, uint64_t lineNum, uint64_t temporalTracking){
	bool found = false;
	//Look for state before making a new one
	for(auto a : stateMap){
		if( (strcmp(a.second->functionName.c_str(), funcName) == 0) && a.second->lineNumber == lineNum){
			//printf("Found state\n");
			activeState = a.second;
			found = true;
			break;    
		}
	}

	if(!found){
		activeState = new state(funcName, lineNum, temporalTracking);
		//printf("making new state: %p\n", activeState);
		stateMap.insert_or_assign(activeState->stateID, activeState);
	} 

	return activeState->stateID;
}


uint64_t EndState(uint64_t stateID){
	if(activeState != nullptr){
		activeState->processStateWindow();
		activeState = activeState->parent;
	}
	else{
		return 1;
	}
	return 0;
}

extern "C" uint64_t CGetState(char* fN, uint64_t lN, uint64_t temporalTracking){
	return GetState(fN, lN, temporalTracking);
}
extern "C" uint64_t CEndState(uint64_t sID){
	return EndState(sID);
}

extern "C" uint64_t CAddToState(void* allocPtr){
	AddToState(allocPtr);
	return 0;
}

//This is for memcalls and for stores/loads using multiple pointers
void AddToStateMulti(void* allocPtr1, void* allocPtr2, uint64_t temporalGraph){
	if(activeState != nullptr){
		AddToState(allocPtr1);
		if(allocPtr2 != 0){
			AddToState(allocPtr2);
		}
	}
}

//This will be put ahead of all store/load instructions during tracking to help build the current state
//If the temporalGraph bool is true, then it is also going to track the temporal access patterns of the state
void AddToState(void* allocPtr){
	//printf("Add to state %p\n", allocPtr);
	if(activeState == nullptr){
		//No active state
		return;
	}
	activeState->stateWindow[activeState->totalStateEntries] = allocPtr;
	activeState->totalStateEntries++;
	if(activeState->totalStateEntries >= activeState->stateWindowSize){
		activeState->processStateWindow();
	}

	return;
}

void PrintJSON(){
	//Catch up to date on Escapes just in case
	processEscapeWindow();

	std::ofstream myfile;
	std::stringstream ss; 
	ss << "program" << ".json";
	std::string fileToOpen = ss.str();
	myfile.open(fileToOpen);
	myfile << "{\"States\": [";

for(auto entry : stateMap){
	auto state = entry.second;
	state->processStateWindow();

	//Print where state starts
	myfile << "\n\t{\"StartPoint\": {\n";
	myfile << "\t\t\"File\": \"" << state->functionName << "\",\n";
	myfile << "\t\t\"Line\": "   << state->lineNumber   << ",\n";
	myfile << "\t},\n";

	//Print where state ends
	myfile << "\t{\"StopPoint\": {\n";
	myfile << "\t\t\"File\": \"" << state->endName    << "\",\n";
	myfile << "\t\t\"Line\": "   << state->endLineNum << ",\n";
	myfile << "\t},\n";

	//Start printing all the allocations
	myfile << "\t\"Allocations\": [\n";

	for(auto alloc : *(state->allocations)){
		myfile << "\t\t{\"VariableName\": " << alloc->variableName;

		std::cout << alloc->variableName << "(" 
		<< std::hex << (uint64_t)alloc->pointer << ")" << "\nOrigin(s):\n";
		for(auto tupleEntry : alloc->origin){
			std::cout << std::dec << std::get<0>(tupleEntry) << ":" << std::get<1>(tupleEntry) << ":" << std::get<2>(tupleEntry) << "\n";
		}
		std::cout << "\n";
	}
}

    

}

void ReportState(uint64_t stateID){
	auto state = stateMap.find(stateID);
	if(state == stateMap.end()){
		printf("STATE NOT FOUND");
		return;
	}
	processEscapeWindow();
    GenerateConnectionGraph();
    state->second->processStateWindow();

	std::ofstream myfile;
	std::stringstream ss; 
	ss << "state" << stateID << ".dot";
	std::string fileToOpen = ss.str();
	myfile.open(fileToOpen);
	myfile << "digraph{\n";

	for(auto alloc : *(state->second->allocations)){
		myfile << std::hex << (uint64_t)alloc->pointer << ";\n";
	}

	if(state->second->temporalGraph){ 
		//Temporal Connections
    	//std::map<allocEntry*, std:map<allocEntry*, uint64_t>> temporalConnections;
		for(auto temporalConnections : state->second->temporalConnections){
			for(auto temporalAlloc : *(temporalConnections.second)){
				myfile << std::hex << (uint64_t)temporalConnections.first->pointer << " -> " << (uint64_t)temporalAlloc.first->pointer << std::dec << "[color=red,label=\"" << temporalAlloc.second << "\",weight=\"" << temporalAlloc.second << "\"];\n";
			}
		}
	}
	//Pointer Connections
    //std::map<allocEntry*, std::map<allocEntry*, uint64_t>*> allocConnections;
	for(auto alloc : *(state->second->allocations)){
        auto allocConnectionMap = allocConnections.find(alloc);
		for(auto pointerAlloc : *(allocConnectionMap->second)){
			myfile << std::hex << (uint64_t)alloc->pointer << " -> " << (uint64_t)pointerAlloc.first->pointer << std::dec << "[label=\"" << pointerAlloc.second << "\",weight=\"" << pointerAlloc.second << "\"];\n";
		}
	}
	myfile << "}";
	myfile.close();
}

void ReportStates(){
	for(auto state : stateMap){
		ReportState(state.second->stateID);
	}
}
