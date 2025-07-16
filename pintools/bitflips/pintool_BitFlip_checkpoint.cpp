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
static KNOB<UINT32> KnobMaxCoeff(
    KNOB_MODE_WRITEONCE, "pintool", "max_coeff", "1",
    "How many coefficients to loop through");

// Global state
static ADDRINT baseAddr    = 0;
static CONTEXT savedCtx;
static bool addressRead    = false;
static bool saved          = false;
static UINT32 currentCoeff = 0;
static UINT32 currentBit   = 0;
static bool hasFlipped     = false;

static UINT64 originalValue;
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
    std::cerr << "[DBG] Read baseAddr=0x" << std::hex << baseAddr << std::dec << std::endl;
    return true;
}

VOID OnLabelHit(CONTEXT* ctxt) {
    if (!addressRead && ReadBaseAddress()) {
        addressRead = true;
        originalValue = *(reinterpret_cast<UINT64*>(baseAddr));
        PIN_SaveContext(ctxt, &savedCtx);
        saved = true;
        std::cerr << "[DBG] Saved context and original value at label" << std::endl;
    }
}

VOID PreInstrFlip(CONTEXT* ctxt, THREADID tid) {
    if (!addressRead || !saved || hasFlipped) return;

    ADDRINT targetEA = baseAddr + currentCoeff * sizeof(UINT64);
    UINT64* ptr = reinterpret_cast<UINT64*>(targetEA);

    // Restaurar el valor original
    *ptr = originalValue;

    std::cerr << "[DBG] Restored coeff value before flip at addr=0x" << std::hex << targetEA << std::endl;

    UINT64 mask = 1ULL << currentBit;
    *ptr ^= mask;
    hasFlipped = true;

    std::cerr << "[INFO] Flipped bit " << currentBit
              << " in coeff " << currentCoeff
              << ", addr=0x" << std::hex << targetEA
              << ", mask=0x" << mask << std::dec << std::endl;
}
// At sync point (e.g., getpid), restore context or move to next
VOID SyscallEntry(THREADID tid, CONTEXT* ctxt, SYSCALL_STANDARD std, VOID* v) {
    ADDRINT syscallCode = PIN_GetSyscallNumber(ctxt, std);
    if (syscallCode != 39 /* SYS_getpid */) return;

    if (!hasFlipped) return;
    hasFlipped = false;

    currentBit++;
    if (currentBit >= 64) {
        currentBit = 0;
        currentCoeff++;
    }

    if (currentCoeff >= KnobMaxCoeff.Value()) {
        std::cerr << "[PIN] Completed all coefficients. Exiting." << std::endl;
        PIN_Detach();
        return;
    }

    PIN_ExecuteAt(&savedCtx);
}

// Instrument the stub label and the specific instruction
VOID ImageCallback(IMG img, VOID*) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::string name = RTN_Name(rtn);

            if (name == KnobLabel.Value()) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnLabelHit), IARG_CONTEXT, IARG_END);
                RTN_Close(rtn);
                std::cerr << "[DBG] Instrumented label: " << name << std::endl;
            }

            else if (name == KnobTargetFunc.Value() && name.find(".cold") == std::string::npos) {
                RTN_Open(rtn);
                UINT32 idx = 0;
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                    if (idx == KnobInstrIndex.Value()) {
                        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(PreInstrFlip), IARG_CONTEXT, IARG_THREAD_ID, IARG_END);
                        std::cerr << "[DBG] Hooked instr index=" << idx << " in func=" << name << std::endl;
                        break;
                    }
                    idx++;
                }
                RTN_Close(rtn);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return 1;
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(ImageCallback, nullptr);
    PIN_AddSyscallEntryFunction(SyscallEntry, nullptr);
    PIN_StartProgram();
    return 0;
}
