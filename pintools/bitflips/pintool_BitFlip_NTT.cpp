#include <fstream>
#include "pin.H"
#include <iostream>

// Knobs to configure the tool
static KNOB<std::string> KnobLabel(
    KNOB_MODE_WRITEONCE, "pintool", "label", "addr_file",
    "Stub function symbol to trigger address read");
static KNOB<std::string> KnobAddrFile(
    KNOB_MODE_WRITEONCE, "pintool", "addr_file", "target_address.txt",
    "File containing written addresses: objectAddr then baseAddr in hex");
static KNOB<std::string> KnobTargetFunc(
    KNOB_MODE_WRITEONCE, "pintool", "func", "foo",
    "Exact symbol name of the internal function to instrument for bitflip");
static KNOB<std::string> KnobFormatFunc(
    KNOB_MODE_WRITEONCE, "pintool", "format_func", "SwitchFormat",
    "Exact symbol name of the formatting function to call before/after bitflip");
static KNOB<UINT32> KnobInstrIndex(
    KNOB_MODE_WRITEONCE, "pintool", "instr_index", "4",
    "Zero-based instruction index within the function where to flip the bit");
static KNOB<UINT32> KnobTargetCoeff(
    KNOB_MODE_WRITEONCE, "pintool", "coeff", "0",
    "Coefficient index to flip (0-based)");
static KNOB<UINT32> KnobTargetBit(
    KNOB_MODE_WRITEONCE, "pintool", "bit", "0",
    "Bit position within the 64-bit word to flip (0-63)");

// Global state
static ADDRINT objectAddr   = 0;
static ADDRINT baseAddr     = 0;
static ADDRINT targetEA     = 0;
static ADDRINT formatFuncAddr = 0;
static bool addressRead     = false;
static bool bitFlipped      = false;

// Read both addresses (objectAddr then baseAddr) from the file
bool ReadAddresses() {
    std::ifstream in(KnobAddrFile.Value());
    if (!in.is_open()) {
        std::cerr << "[ERROR] Cannot open address file: " << KnobAddrFile.Value() << std::endl;
        return false;
    }
    // First line: object address
    in >> std::hex >> objectAddr;
    if (!in) {
        std::cerr << "[ERROR] Failed to parse objectAddr" << std::endl;
        return false;
    }
    // Second line: base address
    in >> std::hex >> baseAddr;
    if (!in) {
        std::cerr << "[ERROR] Failed to parse baseAddr" << std::endl;
        return false;
    }
    // Compute targetEA
    targetEA = baseAddr + UINT64(KnobTargetCoeff.Value()) * sizeof(UINT64);
    std::cerr << "[DBG] Read objectAddr=0x" << std::hex << objectAddr
              << ", baseAddr=0x" << baseAddr
              << " -> targetEA=0x" << targetEA << std::dec << std::endl;
    return true;
}

// Called when hitting the stub label in the target
VOID OnLabelHit() {
    if (!addressRead) {
        if (ReadAddresses()) {
            addressRead = true;
            std::cerr << "[DBG] Addresses read, addressRead=true" << std::endl;
        }
    }
}

// Wrapper that calls formatFunc, flips bit, calls formatFunc again
VOID InvokeWithFormatAndFlip() {
    if (addressRead && !bitFlipped && formatFuncAddr != 0) {
        // Call format function before flip
        typedef void (*FormatFn)(void*);
        FormatFn fmt = (FormatFn)formatFuncAddr;
        fmt(reinterpret_cast<void*>(objectAddr));

        // Perform bit flip
        UINT64* ptr = reinterpret_cast<UINT64*>(targetEA);
        UINT64 mask = (1ULL << KnobTargetBit.Value());
        std::cerr << "[INFO] Flipping bit " << KnobTargetBit.Value()
                  << " at 0x" << std::hex << targetEA
                  << ", mask=0x" << mask << std::dec << std::endl;
        *ptr ^= mask;
        bitFlipped = true;
        std::cerr << "[INFO] Bit flipped successfully" << std::endl;

        // Call format function after flip
        fmt(reinterpret_cast<void*>(objectAddr));
    }
}

// Instrument images: find label, target func, and format func
VOID ImageCallback(IMG img, VOID*) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::string name = RTN_Name(rtn);
            // 1) Instrument stub label
            if (name == KnobLabel.Value()) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnLabelHit), IARG_END);
                RTN_Close(rtn);
                std::cerr << "[DBG] Instrumented label: " << name << std::endl;
            }
            // 2) Capture format function address
            else if (name == KnobFormatFunc.Value()) {
                RTN_Open(rtn);
                formatFuncAddr = RTN_Address(rtn);
                RTN_Close(rtn);
                std::cerr << "[DBG] Captured formatFunc at address=0x" << std::hex << formatFuncAddr << std::dec << std::endl;
            }
            // 3) Instrument the Nth instruction in the target function
            else if (name == KnobTargetFunc.Value()) {
                RTN_Open(rtn);
                UINT32 idx = 0;
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                    if (idx == KnobInstrIndex.Value()) {
                        INS_InsertCall(ins, IPOINT_BEFORE,
                                       AFUNPTR(InvokeWithFormatAndFlip), IARG_END);
                        std::cerr << "[DBG] Inserted format+flip at instr idx=" << idx
                                  << " in function " << name << std::endl;
                        break;
                    }
                    idx++;
                }
                RTN_Close(rtn);
            }
        }
    }
}

// Finalization: warn if no flip
VOID Fini(INT32, VOID*) {
    if (!bitFlipped) std::cerr << "[WARNING] No bit flipped" << std::endl;
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return 1;
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(ImageCallback, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);
    PIN_StartProgram();
    return 0;
}

