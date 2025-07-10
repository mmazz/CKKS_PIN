#include <pin.H>
#include <iostream>
#include <iomanip>

// Knobs para recibir -addr y -bit
static KNOB<ADDRINT> KnobTargetAddr(
    KNOB_MODE_WRITEONCE, "pintool", "addr", "0",
    "Hex address where to flip the bit");

static KNOB<UINT32> KnobTargetBit(
    KNOB_MODE_WRITEONCE, "pintool", "bit", "0",
    "Which bit [0-7] to flip");

// Variable para controlar si ya se ejecutó el flip
static bool bitFlipped = false;

// Función para verificar si la dirección coincide
ADDRINT CheckMemoryAccess(ADDRINT addr)
{
    // Verificar si la dirección accedida coincide con nuestra dirección objetivo
    ADDRINT target = KnobTargetAddr.Value();

    // Verificar rangos (asumiendo que la variable puede ser de varios bytes)
    if (addr >= target && addr < target + 8) {
        std::cerr << "[bitflip] Memory access detected at 0x" << std::hex << addr
                  << " (target: 0x" << target << ")" << std::dec << std::endl;
        return 1;
    }
    return 0;
}

// Handler: invierte el bit en la dirección especificada
VOID FlipBitOnAccess(ADDRINT addr)
{
    // Evitar múltiples ejecuciones
    if (bitFlipped) {
        return;
    }

    // Acceder a la memoria directamente
    UINT8* ptr = reinterpret_cast<UINT8*>(addr);
    UINT8 originalValue = *ptr;
    UINT8 mask = UINT8(1u << KnobTargetBit.Value());

    // Realizar el flip del bit
    *ptr ^= mask;

    bitFlipped = true;

    std::cerr << "[bitflip] SUCCESS: Flipped bit "
              << KnobTargetBit.Value()
              << " at address 0x" << std::hex << addr
              << " (0x" << std::hex << static_cast<int>(originalValue)
              << " -> 0x" << std::hex << static_cast<int>(*ptr) << ")"
              << std::dec << std::endl;
}

// Callback para interceptar accesos a memoria
VOID InstructionCallback(INS ins, VOID*)
{
    // Verificar si la instrucción lee desde nuestra dirección objetivo
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
            IARG_ADDRINT, KnobTargetAddr.Value(),
            IARG_END
        );
    }

    // También verificar si la instrucción escribe a nuestra dirección objetivo
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
            IARG_ADDRINT, KnobTargetAddr.Value(),
            IARG_END
        );
    }
}

// Callback para finalización
VOID Fini(INT32 code, VOID *v)
{
    if (!bitFlipped) {
        std::cerr << "[bitflip] WARNING: Target address 0x"
                  << std::hex << KnobTargetAddr.Value()
                  << " was never accessed!" << std::dec << std::endl;
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

    // Validar parámetros
    if (KnobTargetAddr.Value() == 0) {
        std::cerr << "ERROR: You must specify a target address with -addr" << std::endl;
        return 1;
    }

    if (KnobTargetBit.Value() > 7) {
        std::cerr << "ERROR: Bit position must be between 0 and 7" << std::endl;
        return 1;
    }

    std::cerr << "[bitflip] Target address: 0x" << std::hex << KnobTargetAddr.Value() << std::dec << std::endl;
    std::cerr << "[bitflip] Target bit: " << KnobTargetBit.Value() << std::endl;

    // Inicializar símbolos para mejor debugging
    PIN_InitSymbols();

    // Registrar callbacks
    INS_AddInstrumentFunction(InstructionCallback, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);

    // Iniciar el programa instrumentado
    PIN_StartProgram();

    return 0;
}
