#include <pin.H>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>

// Knob para recibir -bit (mantenemos este)
static KNOB<UINT32> KnobTargetCoeff(
    KNOB_MODE_WRITEONCE, "pintool", "coeff", "0",
    "Which coeff [0-ringDim] to flip. ");

// Knob para recibir -bit (mantenemos este)
static KNOB<UINT32> KnobTargetBit(
    KNOB_MODE_WRITEONCE, "pintool", "bit", "0",
    "Which bit [0-63] to flip");
// Knob para forzar lectura inmediata del archivo (sin esperar label)
static KNOB<bool> KnobForceRead(
    KNOB_MODE_WRITEONCE, "pintool", "force", "false",
    "Force immediate reading of address file without waiting for label");

// Knob para especificar el label donde leer el archivo
static KNOB<std::string> KnobTriggerLabel(
    KNOB_MODE_WRITEONCE, "pintool", "label", "read_address_file",
    "Label name where to read the address file");

static KNOB<std::string> KnobTriggeriFunc(
    KNOB_MODE_WRITEONCE, "pintool", "func", "Encrypt",
    "Function before bit flip");


std::string target_function_name;
bool inside_openfhe_function = false;
bool openfhe_function_found = false;
// Variable para almacenar la dirección leída del archivo
static ADDRINT targetAddress = 0;

// Variable para controlar si ya se ejecutó el flip
static bool bitFlipped = false;

// Variable para controlar si ya se leyó el archivo
static bool addressRead = false;

// Función para leer la dirección desde el archivo
bool ReadTargetAddress()
{
    std::ifstream file("target_address.txt");
    if (!file.is_open()) {
        std::cerr << "[bitflip] ERROR: Cannot open target_address.txt" << std::endl;
        return false;
    }

    std::string line;
    if (std::getline(file, line)) {
        // Limpiar espacios en blanco
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        std::cerr << "[bitflip] DEBUG: Raw line from file: '" << line << "'" << std::endl;

        // Limpiar múltiples prefijos 0x
        while (line.find("0x0x") == 0) {
            line = line.substr(2);
        }

        // Intentar parsear como hexadecimal (con o sin prefijo 0x)
        std::stringstream ss;
        if (line.find("0x") == 0 || line.find("0X") == 0) {
            ss << std::hex << line.substr(2);
        } else {
            // Asumir que es hexadecimal sin prefijo
            ss << std::hex << line;
        }

        if (ss >> targetAddress && targetAddress != 0) {
            std::cerr << "[bitflip] Address read from file: 0x" << std::hex << targetAddress << std::dec << std::endl;
            file.close();
            return true;
        } else {
            std::cerr << "[bitflip] ERROR: Invalid address format in target_address.txt: " << line << std::endl;
        }
    } else {
        std::cerr << "[bitflip] ERROR: target_address.txt is empty" << std::endl;
    }

    file.close();
    return false;
}

// Callback para cuando se alcanza el label trigger
VOID OnTriggerLabel()
{
    if (!addressRead) {
        std::cerr << "[bitflip] Trigger label reached, reading address file..." << std::endl;
        if (ReadTargetAddress()) {
            addressRead = true;
            std::cerr << "[bitflip] File read successfully. Starting bit flip monitoring." << std::endl;
        } else {
            std::cerr << "[bitflip] ERROR: Failed to read address file at trigger point" << std::endl;
        }
    }
}

// Función para verificar si la dirección coincide
ADDRINT CheckMemoryAccess(ADDRINT addr)
{
    // Solo verificar si ya tenemos una dirección válida
    if (!addressRead || targetAddress == 0) {
        return 0;
    }

    // Verificar si la dirección accedida coincide con nuestra dirección objetivo
    // Verificar rangos (asumiendo que la variable puede ser de varios bytes)
    if (addr >= targetAddress && addr < targetAddress + 8) {
        std::cerr << "[bitflip] Memory access detected at 0x" << std::hex << addr
                  << " (target: 0x" << targetAddress << ")" << std::dec << std::endl;
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
    if (!KnobTriggeriFunc.Value().empty() && !inside_openfhe_function) {
        std::cerr << "[bitflip] Abort Bitflip: " << !KnobTriggeriFunc.Value().empty() << " " << !inside_openfhe_function<< std::endl;
        // Si se especificó función OpenFHE pero no estamos dentro, no hacer flip
        return;
    }
    // Acceder a la memoria directamente
    UINT64* ptr = reinterpret_cast<UINT64*>(addr) + KnobTargetCoeff.Value();
    UINT64 originalValue = *ptr;
    UINT64 mask = UINT64(UINT64(1ULL) << KnobTargetBit.Value());

    std::cerr << "[bitflip] Target before bitflip: " << *ptr << std::endl;
    // Realizar el flip del bit
    *ptr ^= mask;

    bitFlipped = true;

    std::cerr << "[bitflip] SUCCESS: Flipped bit "
              << KnobTargetBit.Value() << " target value: " << *ptr
              << " at address 0x" << std::hex << addr
              << " (0x" << std::hex << static_cast<int>(originalValue)
              << " -> 0x" << std::hex << static_cast<int>(*ptr) << ")"
              << std::dec << std::endl;
    inside_openfhe_function = false;
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
            IARG_MEMORYREAD_EA,
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
            IARG_MEMORYWRITE_EA,
            IARG_END
        );
    }
}
// Callback que se ejecuta al entrar a cualquier función
VOID OnOpenFHEFunctionEntry() {
    std::cerr << "[bitflip] Entered OpenFHE function: " << KnobTriggeriFunc.Value() << std::endl;
    inside_openfhe_function = true;
}

VOID ImageCallback(IMG img, VOID*)
{

    std::cerr << "[bitflip] Found OpenFHE function: " << KnobTriggeriFunc.Value()<< std::endl;
    if (!addressRead) {
        std::cerr << "[bitflip] DEBUG: Loading image: " << IMG_Name(img) << std::endl;

        // Buscar en todas las secciones de la imagen
        for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
            for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
                std::string routineName = RTN_Name(rtn);
             //   std::cerr << routineName << std::endl;
                // Buscar trigger label original
                if (routineName == KnobTriggerLabel.Value()) {
                    std::cerr << "[bitflip] Found trigger label in routine: " << routineName << std::endl;
                    RTN_Open(rtn);
                    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnTriggerLabel), IARG_END);
                    RTN_Close(rtn);
                }

                // NUEVA FUNCIONALIDAD: Buscar función OpenFHE
                if (!KnobTriggeriFunc.Value().empty() &&
                    routineName == KnobTriggeriFunc.Value()) {
                    std::cerr << "[bitflip] Found OpenFHE function: " << routineName << std::endl;
                    RTN_Open(rtn);
                    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnOpenFHEFunctionEntry), IARG_END);
                    RTN_Close(rtn);
                    openfhe_function_found = true;
                }
            }
        }
    }
}

// Callback para las rutinas (para detectar el label)
VOID RoutineCallback(RTN rtn, VOID*)
{
    if (!addressRead) {
        RTN_Open(rtn);

        // Buscar el label trigger
        std::string routineName = RTN_Name(rtn);
        //std::cerr << "[bitflip] DEBUG: Found routine: " << routineName << std::endl;

        if (routineName == KnobTriggerLabel.Value()) {
            std::cerr << "[bitflip] Found trigger label: " << routineName << std::endl;

            // Insertar callback al inicio de la rutina
            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnTriggerLabel), IARG_END);
        }

        RTN_Close(rtn);
    }
}

// Callback para finalización
VOID Fini(INT32 code, VOID *v)
{
    if (!addressRead) {
        std::cerr << "[bitflip] WARNING: Trigger label '" << KnobTriggerLabel.Value()
                  << "' was never reached, file was not read!" << std::endl;
    } else if (!bitFlipped && targetAddress != 0) {
        std::cerr << "[bitflip] WARNING: Target address 0x"
                  << std::hex << targetAddress
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
    if (KnobTargetBit.Value() >= 64) {
        std::cerr << "ERROR: Bit position must be between 0 and 63" << std::endl;
        return 1;
    }

    // Validar parámetros
    if (KnobTargetCoeff.Value() >= 65536) {
        std::cerr << "ERROR: Coeff must be between 0 and ringDim" << std::endl;
        return 1;
    }

    // Leer la dirección desde el archivo si se fuerza
    if (KnobForceRead.Value()) {
        std::cerr << "[bitflip] Force mode enabled, reading address file immediately..." << std::endl;
        if (ReadTargetAddress()) {
            addressRead = true;
            std::cerr << "[bitflip] File read successfully in force mode." << std::endl;
        } else {
            std::cerr << "[bitflip] ERROR: Failed to read address file in force mode" << std::endl;
            return 1;
        }
    }

    if (!KnobForceRead.Value()) {
        std::cerr << "[bitflip] Trigger label: " << KnobTriggerLabel.Value() << std::endl;
        std::cerr << "[bitflip] Waiting for trigger label to read address file..." << std::endl;
    }
    std::cerr << "[bitflip] Target bit: " << KnobTargetBit.Value() << std::endl;

    target_function_name = KnobTriggeriFunc.Value();
    if (target_function_name.empty()) {
        std::cerr << "ERROR: Debes especificar una función con -func" << std::endl;
        return -1;
    }
    // Inicializar símbolos para mejor debugging
    PIN_InitSymbols();
    // Registrar callbacks
    IMG_AddInstrumentFunction(ImageCallback, nullptr);
    RTN_AddInstrumentFunction(RoutineCallback, nullptr);
    INS_AddInstrumentFunction(InstructionCallback, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);

    // Iniciar el programa instrumentado
    PIN_StartProgram();

    return 0;
}
