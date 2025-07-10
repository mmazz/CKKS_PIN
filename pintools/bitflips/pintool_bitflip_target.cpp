#include <pin.H>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>

// Knobs para recibir parámetros
static KNOB<ADDRINT> KnobTargetAddr(
    KNOB_MODE_WRITEONCE, "pintool", "addr", "0",
    "Hex address where to flip the bit");

static KNOB<UINT32> KnobTargetBit(
    KNOB_MODE_WRITEONCE, "pintool", "bit", "0",
    "Which bit [0-7] to flip");

static KNOB<string> KnobTargetFunction(
    KNOB_MODE_WRITEONCE, "pintool", "func", "",
    "Function name after which to flip the bit");

static KNOB<string> KnobAddressFile(
    KNOB_MODE_WRITEONCE, "pintool", "addrfile", "",
    "File containing the target address");

static KNOB<BOOL> KnobOnEntry(
    KNOB_MODE_WRITEONCE, "pintool", "onentry", "0",
    "Flip bit on function entry (1) or exit (0)");

// Variables de control
static bool bitFlipped = false;
static ADDRINT dynamicTargetAddr = 0;

// Handler: invierte el bit en la dirección especificada
VOID FlipBitOnTrigger()
{
    if (bitFlipped) {
        return;
    }

    ADDRINT targetAddr = dynamicTargetAddr ? dynamicTargetAddr : KnobTargetAddr.Value();

    if (targetAddr == 0) {
        std::cerr << "[bitflip] ERROR: No target address specified!" << std::endl;
        return;
    }

    // Acceder a la memoria directamente
    UINT64* ptr = reinterpret_cast<UINT64*>(targetAddr);
    UINT64 originalValue = *ptr;
    UINT64 mask = UINT64(1ULL << KnobTargetBit.Value());

    // Realizar el flip del bit
    *ptr ^= mask;

    bitFlipped = true;

    std::cerr << "[bitflip] SUCCESS: Flipped bit "
              << KnobTargetBit.Value()
              << " at address 0x" << std::hex << targetAddr
              << " (0x" << std::hex << originalValue
              << " -> 0x" << std::hex << *ptr << ")"
              << std::dec << std::endl;
}

// Handler para función específica
VOID OnFunctionTrigger()
{
    std::cerr << "[bitflip] Function trigger reached, flipping bit..." << std::endl;
    FlipBitOnTrigger();
}

// Función para verificar si la dirección coincide (para modo de acceso a memoria)
ADDRINT CheckMemoryAccess(ADDRINT addr)
{
    ADDRINT target = dynamicTargetAddr ? dynamicTargetAddr : KnobTargetAddr.Value();

    if (addr >= target && addr < target + 8) {
        std::cerr << "[bitflip] Memory access detected at 0x" << std::hex << addr
                  << " (target: 0x" << target << ")" << std::dec << std::endl;
        return 1;
    }
    return 0;
}

// Handler para accesos a memoria
VOID FlipBitOnAccess()
{
    if (bitFlipped) {
        return;
    }

    ADDRINT targetAddr = dynamicTargetAddr ? dynamicTargetAddr : KnobTargetAddr.Value();
    FlipBitOnTrigger();
}

// Callback para instrumentar instrucciones
VOID InstructionCallback(INS ins, VOID*)
{
    // Si no hay función objetivo, usar el modo de acceso a memoria
    if (KnobTargetFunction.Value().empty()) {
        ADDRINT target = dynamicTargetAddr ? dynamicTargetAddr : KnobTargetAddr.Value();
        if (target == 0) return;

        // Interceptar accesos a memoria
        if (INS_IsMemoryRead(ins)) {
            INS_InsertIfCall(
                ins, IPOINT_BEFORE,
                AFUNPTR(CheckMemoryAccess),
                IARG_MEMORYREAD_EA,
                IARG_END
            );
            INS_InsertThenCall(
                ins, IPOINT_BEFORE,
                AFUNPTR(FlipBitOnAccess),
                IARG_END
            );
        }

        if (INS_IsMemoryWrite(ins)) {
            INS_InsertIfCall(
                ins, IPOINT_BEFORE,
                AFUNPTR(CheckMemoryAccess),
                IARG_MEMORYWRITE_EA,
                IARG_END
            );
            INS_InsertThenCall(
                ins, IPOINT_BEFORE,
                AFUNPTR(FlipBitOnAccess),
                IARG_END
            );
        }
    }
}

// Callback para instrumentar funciones
VOID ImageCallback(IMG img, VOID*)
{
    if (!KnobTargetFunction.Value().empty()) {
        RTN rtn = RTN_FindByName(img, KnobTargetFunction.Value().c_str());
        if (RTN_Valid(rtn)) {
            std::cerr << "[bitflip] Found target function: " << KnobTargetFunction.Value() << std::endl;

            RTN_Open(rtn);

            if (KnobOnEntry.Value()) {
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnFunctionTrigger), IARG_END);
            } else {
                RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(OnFunctionTrigger), IARG_END);
            }

            RTN_Close(rtn);
        }
    }
}

// Función para leer dirección desde archivo
VOID ReadAddressFromFile()
{
    if (!KnobAddressFile.Value().empty()) {
        std::ifstream file(KnobAddressFile.Value().c_str());
        if (file.is_open()) {
            file >> std::hex >> dynamicTargetAddr;
            file.close();
            std::cerr << "[bitflip] Read target address from file: 0x"
                      << std::hex << dynamicTargetAddr << std::dec << std::endl;
        } else {
            std::cerr << "[bitflip] WARNING: Could not open address file: "
                      << KnobAddressFile.Value() << std::endl;
        }
    }
}

// Callback para finalización
VOID Fini(INT32 code, VOID *v)
{
    if (!bitFlipped) {
        std::cerr << "[bitflip] WARNING: Bit flip was never executed!" << std::dec << std::endl;
    }
}

int main(int argc, char* argv[])
{
    // Inicializar PIN
    if (PIN_Init(argc, argv)) {
        std::cerr << "PIN initialization failed!" << std::endl;
        std::cerr << KNOB_BASE::StringKnobSummary() << std::endl;
        return 1;
    }

    // Leer dirección desde archivo si se especificó
    ReadAddressFromFile();

    // Validar parámetros
    ADDRINT targetAddr = dynamicTargetAddr ? dynamicTargetAddr : KnobTargetAddr.Value();
    if (targetAddr == 0 && KnobTargetFunction.Value().empty()) {
        std::cerr << "ERROR: You must specify either -addr, -addrfile, or -func" << std::endl;
        return 1;
    }

    if (KnobTargetBit.Value() > 63) {
        std::cerr << "ERROR: Bit position must be between 0 and 63 for uint64" << std::endl;
        return 1;
    }

    std::cerr << "[bitflip] Target address: 0x" << std::hex << targetAddr << std::dec << std::endl;
    std::cerr << "[bitflip] Target bit: " << KnobTargetBit.Value() << std::endl;
    if (!KnobTargetFunction.Value().empty()) {
        std::cerr << "[bitflip] Target function: " << KnobTargetFunction.Value() << std::endl;
        std::cerr << "[bitflip] Trigger on: " << (KnobOnEntry.Value() ? "entry" : "exit") << std::endl;
    }

    // Inicializar símbolos para mejor debugging
    PIN_InitSymbols();

    // Registrar callbacks
    INS_AddInstrumentFunction(InstructionCallback, nullptr);
    IMG_AddInstrumentFunction(ImageCallback, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);

    // Iniciar el programa instrumentado
    PIN_StartProgram();

    return 0;
}
