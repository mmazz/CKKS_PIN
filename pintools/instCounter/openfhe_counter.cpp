// openfhe_ckks_counter.cpp
#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <stack>
#include <string>
#include <tuple>
#include <iomanip>
#include <vector>

using std::cerr;
using std::cout;
using std::endl;
using std::hex;
using std::dec;
using std::map;
using std::ofstream;
using std::stack;
using std::string;
using std::tie;
using std::vector;

// -----------------------------------------------------------------
// Configuración
// -----------------------------------------------------------------
static const string START_MARKER = "start_measurement";
static const string END_MARKER   = "end_measurement";
static const int CALL_HIERARCHY_DEPTH = 10; // función actual + 3 padres

// -----------------------------------------------------------------
// Estado global
// -----------------------------------------------------------------
static BOOL measuring = FALSE;
static ofstream output_file;
static stack<string> call_stack;

// -----------------------------------------------------------------
// Categorías de instrucciones optimizadas para CKKS
// -----------------------------------------------------------------
enum InstructionType {
    INT_ADD, INT_SUB, INT_MUL, INT_DIV,
    SHIFT_LEFT, SHIFT_RIGHT,
    BITWISE_AND, BITWISE_OR, BITWISE_XOR,
    FLOAT_ADD, FLOAT_SUB, FLOAT_MUL, FLOAT_DIV,
    SSE_PACKED, AVX_PACKED, AVX2_PACKED, AVX512_PACKED,
    VECTOR_INT, VECTOR_FLOAT,
    UNKNOWN_TYPE
};

static string typeToString(InstructionType t) {
    switch(t) {
        case INT_ADD:       return "INT_ADD";
        case INT_SUB:       return "INT_SUB";
        case INT_MUL:       return "INT_MUL";
        case INT_DIV:       return "INT_DIV";
        case SHIFT_LEFT:    return "SHIFT_LEFT";
        case SHIFT_RIGHT:   return "SHIFT_RIGHT";
        case BITWISE_AND:   return "BITWISE_AND";
        case BITWISE_OR:    return "BITWISE_OR";
        case BITWISE_XOR:   return "BITWISE_XOR";
        case FLOAT_ADD:     return "FLOAT_ADD";
        case FLOAT_SUB:     return "FLOAT_SUB";
        case FLOAT_MUL:     return "FLOAT_MUL";
        case FLOAT_DIV:     return "FLOAT_DIV";
        case SSE_PACKED:    return "SSE_PACKED";
        case AVX_PACKED:    return "AVX_PACKED";
        case AVX2_PACKED:   return "AVX2_PACKED";
        case AVX512_PACKED: return "AVX512_PACKED";
        case VECTOR_INT:    return "VECTOR_INT";
        case VECTOR_FLOAT:  return "VECTOR_FLOAT";
        default:            return "UNKNOWN_TYPE";
    }
}

// Clasificación mejorada para operaciones CKKS
static InstructionType classifyInstruction(INS ins) {
    string mnemonic = INS_Mnemonic(ins);
    UINT32 category = INS_Category(ins);

    // Operaciones enteras básicas
    if (mnemonic.find("ADD") == 0 && mnemonic.find("P") != 0) return INT_ADD;
    if (mnemonic.find("SUB") == 0 && mnemonic.find("P") != 0) return INT_SUB;
    if (mnemonic.find("IMUL") == 0 || mnemonic.find("MUL") == 0) return INT_MUL;
    if (mnemonic.find("IDIV") == 0 || mnemonic.find("DIV") == 0) return INT_DIV;

    // Shifts (importantes para OpenFHE)
    if (mnemonic.find("SHL") != string::npos || mnemonic.find("SAL") != string::npos) return SHIFT_LEFT;
    if (mnemonic.find("SHR") != string::npos || mnemonic.find("SAR") != string::npos) return SHIFT_RIGHT;

    // Operaciones bitwise
    if (mnemonic.find("AND") == 0) return BITWISE_AND;
    if (mnemonic.find("OR") == 0)  return BITWISE_OR;
    if (mnemonic.find("XOR") == 0) return BITWISE_XOR;

    // Operaciones de punto flotante
    if (mnemonic.find("FADD") == 0 || mnemonic.find("ADDSS") != string::npos ||
        mnemonic.find("ADDSD") != string::npos) return FLOAT_ADD;
    if (mnemonic.find("FSUB") == 0 || mnemonic.find("SUBSS") != string::npos ||
        mnemonic.find("SUBSD") != string::npos) return FLOAT_SUB;
    if (mnemonic.find("FMUL") == 0 || mnemonic.find("MULSS") != string::npos ||
        mnemonic.find("MULSD") != string::npos) return FLOAT_MUL;
    if (mnemonic.find("FDIV") == 0 || mnemonic.find("DIVSS") != string::npos ||
        mnemonic.find("DIVSD") != string::npos) return FLOAT_DIV;

    // SIMD específico por categoría
    if (category == XED_CATEGORY_AVX512) {
        return AVX512_PACKED;
    }
    if (category == XED_CATEGORY_AVX2) {
        return AVX2_PACKED;
    }
    if (category == XED_CATEGORY_AVX) {
        return AVX_PACKED;
    }
    if (category == XED_CATEGORY_SSE || category == XED_CATEGORY_MMX) {
        return SSE_PACKED;
    }

    // Operaciones vectoriales empaquetadas (importantes para CKKS)
    if (mnemonic.find("PADD") == 0 || mnemonic.find("VPADD") == 0) return VECTOR_INT;
    if (mnemonic.find("PSUB") == 0 || mnemonic.find("VPSUB") == 0) return VECTOR_INT;
    if (mnemonic.find("PMUL") == 0 || mnemonic.find("VPMUL") == 0) return VECTOR_INT;
    if (mnemonic.find("VADDPS") == 0 || mnemonic.find("VADDPD") == 0) return VECTOR_FLOAT;
    if (mnemonic.find("VSUBPS") == 0 || mnemonic.find("VSUBPD") == 0) return VECTOR_FLOAT;
    if (mnemonic.find("VMULPS") == 0 || mnemonic.find("VMULPD") == 0) return VECTOR_FLOAT;

    return UNKNOWN_TYPE;
}

// Estructura para la clave del mapa de contadores
struct CounterKey {
    string instruction_type;
    string current_function;
    string parent_1;
    string parent_2;
    string parent_3;
    string parent_4;
    string parent_5;
    string parent_6;
    string parent_7;
    string parent_8;
    string parent_9;

    bool operator<(const CounterKey& other) const {
        return tie(instruction_type, current_function, parent_1, parent_2, parent_3, parent_4, parent_5, parent_6, parent_7, parent_8, parent_9) <
               tie(other.instruction_type, other.current_function, other.parent_1, other.parent_2, other.parent_3, other.parent_4, other.parent_5, other.parent_6, other.parent_7, other.parent_8, other.parent_9);
    }
};

static map<CounterKey, UINT64> instruction_counts;

// Función para obtener la jerarquía de llamadas actual
static CounterKey getCallHierarchy(const string& instruction_type) {
    CounterKey key;
    key.instruction_type = instruction_type;

    // Inicializar todos los campos
    key.current_function = "UNKNOWN";
    key.parent_1 = "";
    key.parent_2 = "";
    key.parent_3 = "";
    key.parent_4 = "";
    key.parent_5 = "";
    key.parent_6 = "";
    key.parent_7 = "";
    key.parent_8 = "";
    key.parent_9 = "";

    if (!call_stack.empty()) {
        vector<string> hierarchy;

        // Copiar el stack a un vector para acceso fácil
        stack<string> temp_stack = call_stack;
        while (!temp_stack.empty()) {
            hierarchy.push_back(temp_stack.top());
            temp_stack.pop();
        }

        // Asignar jerarquía (el top del stack es la función actual)
        if (hierarchy.size() >= 1) key.current_function = hierarchy[0];
        if (hierarchy.size() >= 2) key.parent_1 = hierarchy[1];
        if (hierarchy.size() >= 3) key.parent_2 = hierarchy[2];
        if (hierarchy.size() >= 4) key.parent_3 = hierarchy[3];
        if (hierarchy.size() >= 5) key.parent_4 = hierarchy[4];
        if (hierarchy.size() >= 6) key.parent_5 = hierarchy[5];
        if (hierarchy.size() >= 7) key.parent_6 = hierarchy[6];
        if (hierarchy.size() >= 8) key.parent_7 = hierarchy[7];
        if (hierarchy.size() >= 9) key.parent_8 = hierarchy[8];
        if (hierarchy.size() >= 10) key.parent_9 = hierarchy[9];
    }

    return key;
}

// Función de análisis llamada por cada instrucción
VOID AnalyzeInstruction(UINT32 instruction_type_val) {
    if (!measuring) return;

    InstructionType type = static_cast<InstructionType>(instruction_type_val);
    string type_str = typeToString(type);

    if (type_str == "UNKNOWN_TYPE") return;

    CounterKey key = getCallHierarchy(type_str);
    instruction_counts[key]++;
}

static const std::vector<std::string> blacklistMang = {
    "malloc",      // atrapar malloc / calloc / free
    "calloc",
    "free",
    "_ZSt",        // todo lo que empiece con std:: (std:: = St)
    "_Znwm",       // operador new (unsigned long) → operator new(unsigned long)
    "_ZdlPv",      // operador delete(void*)
    "_ZN2bl",      // namespace blake2 (algo como _ZN2blake…)
    "sodium",      // si usás libsodium
    // …y agregá más según lo que veas en tus logs con nm o objdump
};

bool is_blacklisted(const std::string &mangled) {
    for (auto &pat : blacklistMang) {
        if (mangled.find(pat) != std::string::npos)
            return true;
    }
    return false;
}

VOID OnRoutineEntry(std::string* name) {
    const std::string &m = *name;
    if (!measuring)// || is_blacklisted(m))
        return;      // no apilarla
    call_stack.push(m);
}

VOID OnRoutineExit() {
    if (measuring && !call_stack.empty()) {
        call_stack.pop();
    }
}

VOID InstrumentRoutine(RTN rtn, VOID* v) {
    string routine_name = RTN_Name(rtn);

    // Detectar marcadores de inicio/fin
    if (routine_name == START_MARKER) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(+[]() {
            measuring = TRUE;
            cout << "[PIN] *** INICIO DE MEDICIÓN ***" << endl;
        }), IARG_END);
        RTN_Close(rtn);
        return;
    }

    if (routine_name == END_MARKER) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(+[]() {
            measuring = FALSE;
            cout << "[PIN] *** FIN DE MEDICIÓN ***" << endl;
        }), IARG_END);
        RTN_Close(rtn);
        return;
    }

    RTN_Open(rtn);

    string* name_ptr = new string(routine_name);
    RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnRoutineEntry),
                   IARG_PTR, name_ptr, IARG_END);

    RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(OnRoutineExit), IARG_END);

    RTN_Close(rtn);
}

VOID InstrumentInstruction(INS ins, VOID* v) {
    InstructionType type = classifyInstruction(ins);
    if (type == UNKNOWN_TYPE) return;

    UINT32 type_val = static_cast<UINT32>(type);

    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(AnalyzeInstruction),
                   IARG_UINT32, type_val, IARG_END);
}

VOID Finish(INT32 code, VOID* v) {
    cout << "[PIN] Escribiendo resultados..." << endl;

    output_file << "Tipo_Instruccion,Conteo,Funcion_Actual,Funcion_Padre_1,Funcion_Padre_2,Funcion_Padre_3,Funcion_Padre_4,Funcion_Padre_5,Funcion_Padre_6,Funcion_Padre_7,Funcion_Padre_8,Funcion_Padre_9,\n";

    for (const auto& entry : instruction_counts) {
        const CounterKey& key = entry.first;
        UINT64 count = entry.second;

        output_file << key.instruction_type << ","
                   << count << ","
                   << key.current_function << ","
                   << key.parent_1 << ","
                   << key.parent_2 << ","
                   << key.parent_3 << ","
                   << key.parent_4 << ","
                   << key.parent_5 << ","
                   << key.parent_6 << ","
                   << key.parent_7 << ","
                   << key.parent_8 << ","
                   << key.parent_9 << "\n";
    }

    output_file.close();
    cout << "[PIN] Resultados escritos en openfhe_ckks_counts.csv" << endl;
    cout << "[PIN] Total de entradas únicas: " << instruction_counts.size() << endl;
}

INT32 Usage() {
    cerr << "Uso: pin -t obj-intel64/openfhe_ckks_counter.so -- <programa_openfhe>" << endl;
    cerr << "El programa debe tener funciones start_measurement() y end_measurement()" << endl;
    return -1;
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) {
        return Usage();
    }

    output_file.open("openfhe_ckks_counts.csv");
    if (!output_file.is_open()) {
        cerr << "[PIN] ERROR: No se pudo abrir el archivo CSV de salida" << endl;
        return 1;
    }

    RTN_AddInstrumentFunction(InstrumentRoutine, nullptr);
    INS_AddInstrumentFunction(InstrumentInstruction, nullptr);
    PIN_AddFiniFunction(Finish, nullptr);

    cout << "[PIN] OpenFHE CKKS Instruction Counter iniciado" << endl;
    cout << "[PIN] Esperando marcadores start_measurement/end_measurement..." << endl;

    PIN_StartProgram();

    return 0;
}
