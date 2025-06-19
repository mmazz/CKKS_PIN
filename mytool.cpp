#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>

using std::cerr;
using std::endl;

// Parámetros
static INT32 index_k = 0;
static INT32 index_i = 0;
static INT32 index_j = 0;
static INT32 bit_index = 0;
static std::string target_function;

class Plaintext;

VOID ModifyCoeff(ADDRINT c_ptr) {
    auto* c = reinterpret_cast<Plaintext*>(c_ptr);
    if (!c) return;

    auto& el = c->GetElements()[index_k].GetAllElements()[index_i][index_j];

    std::cout << "Antes: " << el << std::endl;
    el ^= (1ULL << bit_index);
    std::cout << "Después: " << el << std::endl;
}

VOID ImageLoad(IMG img, VOID* v) {
    RTN rtn = RTN_FindByName(img, target_function.c_str());
    if (RTN_Valid(rtn)) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)ModifyCoeff,
                       IARG_FUNCRET_EXITPOINT_VALUE,
                       IARG_END);
        RTN_Close(rtn);
    }
}

// KNOBs
KNOB<INT32> KnobK(KNOB_MODE_WRITEONCE, "pintool", "k", "0", "Elemento k del vector");
KNOB<INT32> KnobI(KNOB_MODE_WRITEONCE, "pintool", "i", "0", "Fila i del vector");
KNOB<INT32> KnobJ(KNOB_MODE_WRITEONCE, "pintool", "j", "0", "Columna j del vector");
KNOB<INT32> KnobBit(KNOB_MODE_WRITEONCE, "pintool", "bit", "0", "Índice del bit a modificar");
KNOB<string> KnobFunc(KNOB_MODE_WRITEONCE, "pintool", "func", "", "Función que retorna el objeto");

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) {
        cerr << "Uso: pin -t mytool.so -func <func> -k <k> -i <i> -j <j> -bit <b> -- ./tu_binario" << endl;
        return 1;
    }

    index_k = KnobK.Value();
    index_i = KnobI.Value();
    index_j = KnobJ.Value();
    bit_index = KnobBit.Value();
    target_function = KnobFunc.Value();

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_StartProgram();
    return 0;
}
