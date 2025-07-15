#include <fstream>
#include "pin.H"
#include <iostream>

// Knobs to configure the tool
static KNOB<std::string> KnobLabel(
    KNOB_MODE_WRITEONCE, "pintool", "label", "addr_file",
    "Stub function symbol to trigger address read");
static KNOB<std::string> KnobAddrFile(
    KNOB_MODE_WRITEONCE, "pintool", "addr_file", "target_address.txt",
    "File containing written address in hex");
static KNOB<std::string> KnobTargetFunc(
    KNOB_MODE_WRITEONCE, "pintool", "func", "foo",
    "Exact symbol name of the internal function to instrument");
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
static ADDRINT baseAddr    = 0;
static ADDRINT targetEA    = 0;
static bool addressRead    = false;
static bool bitFlipped     = false;

// Read the base address from the file
bool ReadBaseAddress() {
    std::ifstream in(KnobAddrFile.Value());
    if (!in.is_open()) {
        std::cerr << "[ERROR] Cannot open address file: " << KnobAddrFile.Value() << std::endl;
        return false;
    }
    in >> std::hex >> baseAddr;
    if (!in) {
        std::cerr << "[ERROR] Failed to parse hex address" << std::endl;
        return false;
    }
    targetEA = baseAddr + UINT64(KnobTargetCoeff.Value()) * sizeof(UINT64);
    std::cerr << "[DBG] Read baseAddr=0x" << std::hex << baseAddr
              << " -> targetEA=0x" << targetEA << std::dec << std::endl;
    return true;
}

// Called when hitting the stub label in the target
VOID OnLabelHit() {
    if (!addressRead) {
        if (ReadBaseAddress()) {
            addressRead = true;
            std::cerr << "[DBG] AddressRead=true" << std::endl;
        }
    }
}

// Flip the designated bit at targetEA
VOID DoBitFlip() {
    if (addressRead && !bitFlipped) {
        UINT64* ptr = reinterpret_cast<UINT64*>(targetEA);
        UINT64 mask = (1ULL << KnobTargetBit.Value());
        std::cerr << "[INFO] Flipping bit " << KnobTargetBit.Value()
                  << " at 0x" << std::hex << targetEA
                  << ", mask=0x" << mask << std::dec << std::endl;
        *ptr ^= mask;
        bitFlipped = true;
        std::cerr << "[INFO] Bit flipped successfully" << std::endl;
    }
}

// Instrument the stub label and the specific instruction
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
            // 2) Instrument the Nth instruction in the target function
            else if (name == KnobTargetFunc.Value()) {
                RTN_Open(rtn);
                UINT32 idx = 0;
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                    if (idx == KnobInstrIndex.Value()) {
                        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(DoBitFlip), IARG_END);
                        std::cerr << "[DBG] Inserted bitflip at instr idx=" << idx
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
