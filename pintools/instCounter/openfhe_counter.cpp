// openfhe_counter.cpp
#include "pin.H"
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <tuple>
#include <iomanip>

using std::cerr;
using std::cout;
using std::endl;
using std::hex;
using std::map;
using std::ofstream;
using std::string;
using std::tie;

// -----------------------------------------------------------------
// Marker names
// -----------------------------------------------------------------
static const string START_MARKER = "start_measurement";
static const string END_MARKER   = "end_measurement";

// -----------------------------------------------------------------
// Globals
// -----------------------------------------------------------------
static BOOL   measuring    = FALSE;
static ofstream output_file;

// -----------------------------------------------------------------
// Instruction categories
// -----------------------------------------------------------------
enum InstructionCategory {
    INT_ADD, INT_SUB, INT_MUL, INT_DIV,
    SHIFT_LEFT, SHIFT_RIGHT,
    BITWISE_AND, BITWISE_OR, BITWISE_XOR,
    FLOAT_ADD, FLOAT_SUB, FLOAT_MUL, FLOAT_DIV,
    SSE_PACKED, AVX_PACKED, AVX2_PACKED, AVX512_PACKED,
    OTHER_SIMD, UNKNOWN
};

static string categoryToString(InstructionCategory c) {
    switch(c) {
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
      case OTHER_SIMD:    return "OTHER_SIMD";
      default:            return "UNKNOWN";
    }
}

static InstructionCategory classifyInstruction(INS ins) {
    string op = INS_Mnemonic(ins);
    UINT32 cat = INS_Category(ins);
    if (op.rfind("ADD",0)==0 && op.rfind("P",0)!=0) return INT_ADD;
    if (op.rfind("SUB",0)==0 && op.rfind("P",0)!=0) return INT_SUB;
    if (op.find("MUL")!=string::npos && op.rfind("PMUL",0)!=0) return INT_MUL;
    if (op.find("DIV")!=string::npos) return INT_DIV;
    if (op.find("SHL")!=string::npos||op.find("SAL")!=string::npos) return SHIFT_LEFT;
    if (op.find("SHR")!=string::npos||op.find("SAR")!=string::npos) return SHIFT_RIGHT;
    if (op.rfind("AND",0)==0) return BITWISE_AND;
    if (op.rfind("OR",0)==0)  return BITWISE_OR;
    if (op.rfind("XOR",0)==0) return BITWISE_XOR;
    if (op.rfind("FADD",0)==0||op.find("ADDS")!=string::npos) return FLOAT_ADD;
    if (op.rfind("FSUB",0)==0||op.find("SUBS")!=string::npos) return FLOAT_SUB;
    if (op.rfind("FMUL",0)==0||op.find("MULS")!=string::npos) return FLOAT_MUL;
    if (op.rfind("FDIV",0)==0||op.find("DIVS")!=string::npos) return FLOAT_DIV;
    if (cat==XED_CATEGORY_SSE||cat==XED_CATEGORY_MMX)  return SSE_PACKED;
    if (cat==XED_CATEGORY_AVX||cat==XED_CATEGORY_AVX2) return AVX_PACKED;
    if (cat==XED_CATEGORY_AVX512)                     return AVX512_PACKED;
    if (op.find("XMM")!=string::npos||op.find("YMM")!=string::npos||op.find("ZMM")!=string::npos)
        return OTHER_SIMD;
    return UNKNOWN;
}

struct InstrKey {
    ADDRINT func_addr;
    string func_name;
    string category;
    bool operator<(InstrKey const& o) const {
        return tie(func_addr,func_name,category) < tie(o.func_addr,o.func_name,o.category);
    }
};
static map<InstrKey, UINT64> counts;

VOID Analyze(ADDRINT ip, ADDRINT func_addr, string* func_name, UINT32 cat_val) {
    if (!measuring) return;
    auto cat = static_cast<InstructionCategory>(cat_val);
    string cat_str = categoryToString(cat);
    if (cat_str=="UNKNOWN") return;
    counts[{func_addr,*func_name,cat_str}]++;
}

VOID Routine(RTN rtn, VOID* v) {
    string name = RTN_Name(rtn);
    if (name==START_MARKER || name==END_MARKER) {
        RTN_Open(rtn);
        if (name==START_MARKER) {
            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(+[]() {
                measuring = TRUE;
                cout << "[PIN] Started measurement" << endl;
            }), IARG_END);
        } else {
            RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(+[]() {
                measuring = FALSE;
                cout << "[PIN] Ended measurement" << endl;
            }), IARG_END);
        }
        RTN_Close(rtn);
    }
}

VOID Instruction(INS ins, VOID* v) {
    auto cat = classifyInstruction(ins);
    if (cat==UNKNOWN) return;

    RTN rtn = RTN_FindByAddress(INS_Address(ins));
    string fn = RTN_Valid(rtn)?RTN_Name(rtn):"UNKNOWN";
    ADDRINT fa  = RTN_Valid(rtn)?RTN_Address(rtn):0;
    string* fnp = new string(fn);
    UINT32 cat_val = static_cast<UINT32>(cat);

    INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(Analyze),
                   IARG_INST_PTR,
                   IARG_ADDRINT, fa,
                   IARG_PTR, fnp,
                   IARG_UINT32, cat_val,
                   IARG_END);
}

VOID Fini(INT32 code, VOID* v) {
    output_file << "Function,Address,Instruction_Type,Count\n";
    for (auto const& kv: counts) {
        output_file<<kv.first.func_name<<","<<hex<<kv.first.func_addr
                   <<","<<kv.first.category<<","<<dec<<kv.second<<"\n";
    }
    cout << "[PIN] Results written to openfhe_instruction_counts.csv"<<endl;
    output_file.close();
}

INT32 Usage() {
    cerr<<"Usage: pin -t obj-intel64/openfhe_counter.so -- <program>"<<endl;
    return -1;
}

int main(int argc, char* argv[]) {
    PIN_InitSymbols();  // <--- Load symbol table for RTN_FindByName
    if (PIN_Init(argc, argv)) return Usage();

    output_file.open("openfhe_instruction_counts.csv");
    if (!output_file.is_open()) {
        cerr << "[PIN] ERROR opening CSV" << endl;
        return 1;
    }

    RTN_AddInstrumentFunction(Routine, nullptr);
    INS_AddInstrumentFunction(Instruction, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);

    cout<<"[PIN] OpenFHE instruction counter started"<<endl;
    PIN_StartProgram();
    return 0;
}
