#include "pin.H"
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>

// Knobs
static KNOB<std::string> KnobLabel(
    KNOB_MODE_WRITEONCE, "pintool", "label", "addr_label",
    "Function label to trigger address read");
static KNOB<std::string> KnobAddrFile(
    KNOB_MODE_WRITEONCE, "pintool", "addr_file", "target_address.txt",
    "File with object and base addresses");
static KNOB<std::string> KnobTargetFunc(
    KNOB_MODE_WRITEONCE, "pintool", "func", "foo",
    "Target function for bitflip");
static KNOB<std::string> KnobFormatFunc(
    KNOB_MODE_WRITEONCE, "pintool", "format_func", "SwitchFormat",
    "Formatting function name");
static KNOB<UINT32> KnobInstrIndex(
    KNOB_MODE_WRITEONCE, "pintool", "instr_index", "4",
    "Instruction index for bitflip");
static KNOB<UINT32> KnobTargetCoeff(
    KNOB_MODE_WRITEONCE, "pintool", "coeff", "0",
    "Coefficient index to flip");
static KNOB<UINT32> KnobTargetBit(
    KNOB_MODE_WRITEONCE, "pintool", "bit", "0",
    "Bit position to flip (0-63)");

// Global state
typedef void (*FormatFn)(void*);
static FormatFn fmt = nullptr;
static ADDRINT objectAddr = 0;
static ADDRINT baseAddr   = 0;
static ADDRINT targetEA   = 0;
static bool addressRead   = false;
static bool bitFlipped    = false;

// Read addresses with low-overhead C I/O
static bool ReadAddresses() {
    int fd = open(KnobAddrFile.Value().c_str(), O_RDONLY);
    if (fd < 0) return false;
    char buf[128] = {0};
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0) return false;
    // Parse two hex values: objectAddr then baseAddr
    if (sscanf(buf, "%lx %lx", &objectAddr, &baseAddr) != 2) return false;
    targetEA = baseAddr + KnobTargetCoeff.Value() * sizeof(UINT64);
    addressRead = true;
    return true;
}

// Called at stub label
VOID OnLabelHit() {
    if (!addressRead) {
        ReadAddresses();
    }
}

// Call formatting function (NTT)
VOID CallFormat() {
    if (fmt && addressRead) {
        fmt(reinterpret_cast<void*>(objectAddr));
    }
}

// Perform one-time bit flip
VOID DoBitFlip() {
    if (addressRead && !bitFlipped) {
        UINT64* ptr = reinterpret_cast<UINT64*>(targetEA);
        UINT64 mask = (1ULL << KnobTargetBit.Value());
        UINT64 oldv = *ptr;
        *ptr ^= mask;
        (void)oldv; // suppress unused warning
        bitFlipped = true;
    }
}

// Instrumentation
VOID ImageCallback(IMG img, VOID*) {
    // Capture format function ptr once
    if (!fmt) {
        RTN rf = RTN_FindByName(img, KnobFormatFunc.Value().c_str());
        if (RTN_Valid(rf)) {
            fmt = reinterpret_cast<FormatFn>(RTN_Address(rf));
        }
    }

    // Hook stub label
    RTN rl = RTN_FindByName(img, KnobLabel.Value().c_str());
    if (RTN_Valid(rl)) {
        RTN_Open(rl);
        RTN_InsertCall(rl, IPOINT_BEFORE, AFUNPTR(OnLabelHit), IARG_END);
        RTN_Close(rl);
    }

    // Instrument target function at specific instruction
    RTN rt = RTN_FindByName(img, KnobTargetFunc.Value().c_str());
    if (RTN_Valid(rt)) {
        RTN_Open(rt);
        INS ins = RTN_InsHead(rt);
        for (UINT32 i = 0; i < KnobInstrIndex.Value(); ++i) {
            ins = INS_Next(ins);
        }
        // Before: format + flip
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CallFormat), IARG_END);
        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(DoBitFlip), IARG_END);
        // After: format again
        if (INS_IsValidForIpointAfter(ins)) {
            INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(CallFormat), IARG_END);
        } else if (INS_IsValidForIpointTakenBranch(ins)) {
            INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(CallFormat), IARG_END);
        } else {
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CallFormat), IARG_END);
        }
        RTN_Close(rt);
    }
}

// Finalization
VOID Fini(INT32, VOID*) {
    if (!bitFlipped) {
        // Optionally report
    }
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return 1;
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(ImageCallback, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);
    PIN_StartProgram();
    return 0;
}
