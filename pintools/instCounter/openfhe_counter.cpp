#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <iomanip>
#include <set>

using std::cerr;
using std::cout;
using std::endl;
using std::hex;
using std::map;
using std::ofstream;
using std::pair;
using std::set;
using std::string;

// Global state
static bool measuring = false;
static ofstream output_file;

// Marker names
static const string START_MARKER_FUNC = "start_measurement";
static const string END_MARKER_FUNC = "end_measurement";

// Instruction classification
enum InstructionCategory {
    INTEGER_ADD, INTEGER_SUB, INTEGER_MUL, INTEGER_DIV,
    SHIFT_LEFT, SHIFT_RIGHT,
    BITWISE_AND, BITWISE_OR, BITWISE_XOR,
    FLOAT_ADD, FLOAT_SUB, FLOAT_MUL, FLOAT_DIV,
    SSE_PACKED, AVX_PACKED, AVX2_PACKED, AVX512_PACKED,
    OTHER_SIMD, OTHER_ARITHMETIC, UNKNOWN
};

// Key for instruction counter: <function_address, function_name, category>
struct InstrKey {
    ADDRINT func_addr;
    string func_name;
    string category;
    bool operator<(const InstrKey& other) const {
        return std::tie(func_addr, func_name, category) < std::tie(other.func_addr, other.func_name, other.category);
    }
};

static map<InstrKey, UINT64> instruction_counts;

// Convert category to string
string categoryToString(InstructionCategory cat) {
    switch(cat) {
        case INTEGER_ADD: return "INT_ADD";
        case INTEGER_SUB: return "INT_SUB";
        case INTEGER_MUL: return "INT_MUL";
        case INTEGER_DIV: return "INT_DIV";
        case SHIFT_LEFT: return "SHIFT_LEFT";
        case SHIFT_RIGHT: return "SHIFT_RIGHT";
        case BITWISE_AND: return "BITWISE_AND";
        case BITWISE_OR: return "BITWISE_OR";
        case BITWISE_XOR: return "BITWISE_XOR";
        case FLOAT_ADD: return "FLOAT_ADD";
        case FLOAT_SUB: return "FLOAT_SUB";
        case FLOAT_MUL: return "FLOAT_MUL";
        case FLOAT_DIV: return "FLOAT_DIV";
        case SSE_PACKED: return "SSE_PACKED";
        case AVX_PACKED: return "AVX_PACKED";
        case AVX2_PACKED: return "AVX2_PACKED";
        case AVX512_PACKED: return "AVX512_PACKED";
        case OTHER_SIMD: return "OTHER_SIMD";
        case OTHER_ARITHMETIC: return "OTHER_ARITHMETIC";
        default: return "UNKNOWN";
    }
}
InstructionCategory classifyInstruction(INS ins) {
    const string op = INS_Mnemonic(ins);
    const UINT32 cat = INS_Category(ins);

    // Integer arithmetic
    if (op.find("ADD") == 0 && op.find("P") != 0) return INTEGER_ADD;
    if (op.find("SUB") == 0 && op.find("P") != 0) return INTEGER_SUB;
    if (op.find("MUL") != string::npos && op.find("PMUL") != 0) return INTEGER_MUL;
    if (op.find("DIV") != string::npos) return INTEGER_DIV;

    if (op.find("SHL") != string::npos || op.find("SAL") != string::npos) return SHIFT_LEFT;
    if (op.find("SHR") != string::npos || op.find("SAR") != string::npos) return SHIFT_RIGHT;

    if (op.find("AND") == 0) return BITWISE_AND;
    if (op.find("OR") == 0) return BITWISE_OR;
    if (op.find("XOR") == 0) return BITWISE_XOR;

    // FP instructions (x87 or SIMD)
    if (op.find("FADD") == 0 || op.find("ADDS") != string::npos) return FLOAT_ADD;
    if (op.find("FSUB") == 0 || op.find("SUBS") != string::npos) return FLOAT_SUB;
    if (op.find("FMUL") == 0 || op.find("MULS") != string::npos) return FLOAT_MUL;
    if (op.find("FDIV") == 0 || op.find("DIVS") != string::npos) return FLOAT_DIV;

    if (cat == XED_CATEGORY_AVX512) return AVX512_PACKED;
    if (cat == XED_CATEGORY_AVX || cat == XED_CATEGORY_AVX2) return AVX_PACKED;
    if (cat == XED_CATEGORY_SSE || cat == XED_CATEGORY_MMX) return SSE_PACKED;

    if (op.find("XMM") != string::npos || op.find("YMM") != string::npos || op.find("ZMM") != string::npos)
        return OTHER_SIMD;

    return UNKNOWN;
}

VOID AnalyzeInstruction(ADDRINT addr, ADDRINT rtn_addr, string* rtn_name, UINT32 category_val) {
    if (!measuring) return;

    InstructionCategory category = static_cast<InstructionCategory>(category_val);
    string category_str = categoryToString(category);
    if (category_str == "UNKNOWN") return;

    InstrKey key = { rtn_addr, *rtn_name, category_str };
    instruction_counts[key]++;
}

// Marker function hooks
VOID OnFunctionEntry(ADDRINT addr, string* func_name) {
    if (*func_name == START_MARKER_FUNC) {
        measuring = true;
        cout << "[PIN] Started measurement" << endl;
    } else if (*func_name == END_MARKER_FUNC) {
        measuring = false;
        cout << "[PIN] Ended measurement" << endl;
    }
}

// Instruction instrumentation
VOID Instruction(INS ins, VOID* v) {
    InstructionCategory category = classifyInstruction(ins);
    if (category == UNKNOWN) return;

    RTN rtn = RTN_FindByAddress(INS_Address(ins));
    string func_name = (RTN_Valid(rtn)) ? RTN_Name(rtn) : "UNKNOWN";
    ADDRINT func_addr = (RTN_Valid(rtn)) ? RTN_Address(rtn) : 0;

    string* fn = new string(func_name);
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeInstruction,
               IARG_INST_PTR,
               IARG_ADDRINT, func_addr,
               IARG_PTR, fn,
               IARG_UINT32, category,  // NUEVO
               IARG_END);

}

// Hook each function via image loading
VOID ImageLoad(IMG img, VOID* v) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            string name = RTN_Name(rtn);
            if (name == START_MARKER_FUNC || name == END_MARKER_FUNC) {
                RTN_Open(rtn);
                string* ptr = new string(name);
                RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)OnFunctionEntry,
                               IARG_INST_PTR, IARG_PTR, ptr, IARG_END);
                RTN_Close(rtn);
            }
        }
    }
}

// Write results to CSV
VOID Fini(INT32 code, VOID* v) {
    output_file << "Function,Address,Instruction_Type,Count" << endl;
    for (const auto& kv : instruction_counts) {
        output_file << kv.first.func_name << ","
                    << std::hex << kv.first.func_addr << ","
                    << kv.first.category << ","
                    << std::dec << kv.second << endl;
    }
    cout << "[PIN] Results written to openfhe_instruction_counts.csv" << endl;
    output_file.close();
}

// Usage
INT32 Usage() {
    cerr << "Usage: pin -t obj-intel64/openfhe_counter.so -- <your_program>" << endl;
    return -1;
}

// Main
int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return Usage();

    output_file.open("openfhe_instruction_counts.csv");
    if (!output_file.is_open()) {
        cerr << "Failed to open output file." << endl;
        return 1;
    }

    INS_AddInstrumentFunction(Instruction, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddFiniFunction(Fini, 0);

    cout << "[PIN] OpenFHE instruction counter started" << endl;
    PIN_StartProgram();

    return 0;
}

