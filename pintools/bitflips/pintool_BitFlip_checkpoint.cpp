#include "pin.H"
#include <fstream>
#include <iostream>
#include <unistd.h> // for getpid()
#include <sys/syscall.h> // for SYS_getpid

// Knobs to configure the tool
static KNOB<std::string> KnobAddrFile(
    KNOB_MODE_WRITEONCE, "pintool", "addr_file", "target_address.txt",
    "Path to file containing written base address in hex");
static KNOB<UINT32> KnobMaxCoeff(
    KNOB_MODE_WRITEONCE, "pintool", "max_coeff", "1",
    "Number of coefficients to iterate");

// Global state
static ADDRINT baseAddr = 0;
static UINT32 currentCoeff = 0;
static UINT32 currentBit = 0;

// Read base address from file once
void ReadBaseAddr() {
    if (baseAddr != 0) return;
    std::ifstream in(KnobAddrFile.Value());
    if (!in.is_open()) {
        std::cerr << "[ERROR] Cannot open address file: " << KnobAddrFile.Value() << std::endl;
        PIN_ExitProcess(1);
    }
    in >> std::hex >> baseAddr;
    if (!in) {
        std::cerr << "[ERROR] Failed to parse hex address" << std::endl;
        PIN_ExitProcess(1);
    }
    std::cerr << "[DBG] Read baseAddr=0x" << std::hex << baseAddr << std::dec << std::endl;
}

// Syscall entry callback: intercept getpid
VOID SyscallEntry(THREADID tid, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID*) {
    UINT32 syscallCode = PIN_GetSyscallNumber(ctxt, std);
    if (syscallCode != SYS_getpid) return;

    ReadBaseAddr();

    // Compute target element and flip bit
    ADDRINT targetEA = baseAddr + currentCoeff * sizeof(UINT64);
    UINT64* ptr = reinterpret_cast<UINT64*>(targetEA);
    UINT64 mask = 1ULL << currentBit;
    *ptr ^= mask;

    std::cerr << "[PIN] Flipped bit " << currentBit
              << " in coeff " << currentCoeff
              << ", addr=0x" << std::hex << targetEA
              << ", mask=0x" << mask << std::dec << std::endl;

    // Advance indices
    currentBit++;
    if (currentBit >= 64) {
        currentBit = 0;
        currentCoeff++;
        if (currentCoeff >= KnobMaxCoeff.Value()) {
            std::cerr << "[PIN] Completed all coefficients. Detaching." << std::endl;
            // Detach and let application continue to write output
            PIN_Detach();
        }
    }
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return 1;
    PIN_InitSymbols();

    // Intercept getpid to trigger flips
    PIN_AddSyscallEntryFunction(SyscallEntry, nullptr);

    PIN_StartProgram(); // Never returns until detached
    return 0;
}

