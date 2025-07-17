#include <fstream>
#include "pin.H"
#include <iostream>
#include <signal.h>
#include <setjmp.h>

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
static ADDRINT objectAddr     = 0;
static ADDRINT baseAddr       = 0;
static ADDRINT targetEA       = 0;
static ADDRINT formatFuncAddr = 0;
static bool addressRead       = false;
static bool bitFlipped        = false;

// Signal handling for crash protection
static jmp_buf crash_jmp;
static volatile sig_atomic_t crash_occurred = 0;

void crash_handler(int sig) {
    crash_occurred = 1;
    longjmp(crash_jmp, 1);
}

// Read both addresses (objectAddr then baseAddr) from the file
bool ReadAddresses() {
    std::ifstream in(KnobAddrFile.Value());
    if (!in.is_open()) {
        std::cerr << "[ERROR] Cannot open address file: " << KnobAddrFile.Value() << std::endl;
        return false;
    }
    in >> std::hex >> objectAddr;
    if (!in) { std::cerr << "[ERROR] Failed to parse objectAddr" << std::endl; return false; }
    in >> std::hex >> baseAddr;
    if (!in) { std::cerr << "[ERROR] Failed to parse baseAddr" << std::endl; return false; }
    targetEA = baseAddr + static_cast<ADDRINT>(KnobTargetCoeff.Value()) * sizeof(UINT64);
    std::cerr << "[DBG] objectAddr=0x" << std::hex << objectAddr
              << ", baseAddr=0x" << baseAddr
              << " -> targetEA=0x" << targetEA << std::dec << std::endl;
    return true;
}

// Called when hitting the stub label in the target
VOID OnLabelHit() {
    if (!addressRead && ReadAddresses()) {
        addressRead = true;
        std::cerr << "[DBG] Addresses read successfully" << std::endl;
    }
}

// Call format function on the object - BEFORE bitflip
VOID CallFormatBefore() {
    if (formatFuncAddr && addressRead) {
        std::cerr << "[DBG] Calling format function BEFORE bitflip" << std::endl;

        // Verificar que la dirección del objeto es válida
        if (objectAddr == 0 || objectAddr < 0x1000) {
            std::cerr << "[ERROR] Invalid object address: 0x" << std::hex << objectAddr << std::dec << std::endl;
            return;
        }

        // Leer algunos bytes del objeto para verificar que es válido
        std::cerr << "[DBG] Object at 0x" << std::hex << objectAddr << " first 8 bytes: ";
        uint64_t* obj_ptr = reinterpret_cast<uint64_t*>(objectAddr);
        for (int i = 0; i < 1; i++) {
            std::cerr << "0x" << std::hex << obj_ptr[i] << " ";
        }
        std::cerr << std::dec << std::endl;

        // Asumiendo que SwitchFormat toma un puntero al objeto
        typedef void (*FormatFn)(void*);
        FormatFn fmt = reinterpret_cast<FormatFn>(formatFuncAddr);

        // Configurar handler de señal para SIGSEGV
        signal(SIGSEGV, crash_handler);
        crash_occurred = 0;

        if (setjmp(crash_jmp) == 0) {
            fmt(reinterpret_cast<void*>(objectAddr));
            std::cerr << "[DBG] Format function BEFORE completed successfully" << std::endl;
        } else {
            std::cerr << "[ERROR] Crash in format function (BEFORE)" << std::endl;
        }

        // Restaurar handler por defecto
        signal(SIGSEGV, SIG_DFL);
    }
}

// Call format function on the object - AFTER bitflip
VOID CallFormatAfter() {
    if (formatFuncAddr && addressRead) {
        std::cerr << "[DBG] Calling format function AFTER bitflip" << std::endl;

        if (objectAddr == 0 || objectAddr < 0x1000) {
            std::cerr << "[ERROR] Invalid object address: 0x" << std::hex << objectAddr << std::dec << std::endl;
            return;
        }

        typedef void (*FormatFn)(void*);
        FormatFn fmt = reinterpret_cast<FormatFn>(formatFuncAddr);

        signal(SIGSEGV, crash_handler);
        crash_occurred = 0;

        if (setjmp(crash_jmp) == 0) {
            fmt(reinterpret_cast<void*>(objectAddr));
            std::cerr << "[DBG] Format function AFTER completed successfully" << std::endl;
        } else {
            std::cerr << "[ERROR] Crash in format function (AFTER)" << std::endl;
        }

        signal(SIGSEGV, SIG_DFL);
    }
}

// Perform the bit flip
VOID DoBitFlip() {
    if (addressRead && !bitFlipped) {
        std::cerr << "[DBG] Performing bitflip" << std::endl;
        UINT64* ptr = reinterpret_cast<UINT64*>(targetEA);
        UINT64 mask = (1ULL << KnobTargetBit.Value());

        std::cerr << "[INFO] Flipping bit " << KnobTargetBit.Value()
                  << " at 0x" << std::hex << targetEA
                  << ", mask=0x" << mask << std::dec << std::endl;

        // Verificar que la dirección es válida antes de escribir
        signal(SIGSEGV, crash_handler);
        crash_occurred = 0;

        if (setjmp(crash_jmp) == 0) {
            UINT64 old_value = *ptr;
            *ptr ^= mask;
            UINT64 new_value = *ptr;
            std::cerr << "[INFO] Bit flipped: 0x" << std::hex << old_value
                      << " -> 0x" << new_value << std::dec << std::endl;
            bitFlipped = true;
        } else {
            std::cerr << "[ERROR] Crash during bitflip at 0x" << std::hex << targetEA << std::dec << std::endl;
        }

        signal(SIGSEGV, SIG_DFL);
    }
}

// Instrumentation: stub label, capture format func address, and sequence around target instruction
VOID ImageCallback(IMG img, VOID*) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::string name = RTN_Name(rtn);
            // 1) stub label to read addresses
            if (name == KnobLabel.Value()) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnLabelHit), IARG_END);
                RTN_Close(rtn);
                std::cerr << "[DBG] Instrumented label: " << name << std::endl;
            }
            // 2) capture format function address
            else if (name == KnobFormatFunc.Value()) {
                RTN_Open(rtn);
                formatFuncAddr = RTN_Address(rtn);
                RTN_Close(rtn);
                std::cerr << "[DBG] Captured format func at 0x" << std::hex << formatFuncAddr << std::dec << std::endl;
            }
            // 3) in target function, wrap around the specific instruction
            else if (name == KnobTargetFunc.Value()) {
                RTN_Open(rtn);
                UINT32 idx = 0;
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                    if (idx == KnobInstrIndex.Value()) {
                        // ANTES de la instrucción: format + bitflip
                        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CallFormatBefore), IARG_END);
                        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(DoBitFlip), IARG_END);

                        // DESPUÉS de la instrucción: depende del tipo
                        if (INS_IsValidForIpointAfter(ins)) {
                            INS_InsertCall(ins, IPOINT_AFTER, AFUNPTR(CallFormatAfter), IARG_END);
                        } else if (INS_IsValidForIpointTakenBranch(ins)) {
                            INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, AFUNPTR(CallFormatAfter), IARG_END);
                        } else {
                            // Para instrucciones que no soportan AFTER ni TAKEN_BRANCH
                            // Ejecutar todo BEFORE en orden
                            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(CallFormatAfter), IARG_END);
                        }

                        std::cerr << "[DBG] Wrapped format+flip around instr idx=" << idx
                                  << " (valid_after=" << INS_IsValidForIpointAfter(ins) << ")" << std::endl;
                        break;
                    }
                    idx++;
                }
                RTN_Close(rtn);
            }
        }
    }
}

// Finalization: warn if no flip happened
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

