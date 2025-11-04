#include "pin.H"
#include <iostream>
#include <atomic>
#include <string>

std::atomic<uint64_t> counterScalar{0};
std::atomic<uint64_t> counterSIMD{0};
bool active_profiling = false;

// ---------------- Clasificación de instrucciones ----------------

bool ScalarArithmetic(UINT32 opcode) {
    return (opcode == XED_ICLASS_ADD);
}

bool SIMDArithmetic(UINT32 opcode) {
    return (opcode == XED_ICLASS_ADDPS  ||
            opcode == XED_ICLASS_ADDPD  ||
            opcode == XED_ICLASS_VADDPS ||
            opcode == XED_ICLASS_VADDPD ||
            opcode == XED_ICLASS_PADDD  ||
            opcode == XED_ICLASS_PADDQ);
}

// ---------------- Handlers de profiling ----------------

VOID ActivarProfiling() {
    active_profiling = true;
    std::cout << "[PIN] Profiling ACTIVADO" << std::endl;
}

VOID DesactivarProfiling() {
    active_profiling = false;
    std::cout << "[PIN] Profiling DESACTIVADO" << std::endl;
}

// ---------------- Contadores ----------------

VOID ContarAddScalar() {
    counterScalar++;
}

VOID ContarAddSIMD() {
    counterSIMD++;
}

VOID ContarCondicional(UINT32 opcode) {
    if (!active_profiling)
        return;

    if (ScalarArithmetic(opcode))
        counterScalar++;
    else if (SIMDArithmetic(opcode))
        counterSIMD++;
}

// ---------------- Instrumentación ----------------

VOID InstAnalizer(INS ins) {
    UINT32 opcode = INS_Opcode(ins);

    if (ScalarArithmetic(opcode) || SIMDArithmetic(opcode)) {
        INS_InsertCall(ins, IPOINT_BEFORE,
                       (AFUNPTR)ContarCondicional,
                       IARG_UINT32, opcode,
                       IARG_END);
    }
}

VOID RutinaEjecutada(std::string rtn_name) {
    if (active_profiling) {
        std::cout << "[PROFILING] Ejecutando rutina: " << rtn_name << std::endl;
    }
}

bool EsRutinaPropia(const std::string &name) {
    return !(name.find("libc") != std::string::npos ||
             name.find("__") == 0 ||
             name.find("std::") != std::string::npos ||
             name.find("new") != std::string::npos ||
             name.find("delete") != std::string::npos ||
             name.find("malloc") != std::string::npos ||
             name.find("free") != std::string::npos);
}



VOID ImageLoad(IMG img, VOID *v) {
    // Solo instrumentar el ejecutable principal
    if (!IMG_IsMainExecutable(img))
        return;

    // Buscar funciones marcadoras
    RTN startRtn = RTN_FindByName(img, "start_profiling_marker");
    if (RTN_Valid(startRtn)) {
        RTN_Open(startRtn);
        RTN_InsertCall(startRtn, IPOINT_BEFORE, (AFUNPTR)ActivarProfiling, IARG_END);
        RTN_Close(startRtn);
    }

    RTN stopRtn = RTN_FindByName(img, "stop_profiling_marker");
    if (RTN_Valid(stopRtn)) {
        RTN_Open(stopRtn);
        RTN_InsertCall(stopRtn, IPOINT_BEFORE, (AFUNPTR)DesactivarProfiling, IARG_END);
        RTN_Close(stopRtn);
    }

    // Instrumentar el resto de las rutinas del main ejecutable
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::string name = PIN_UndecorateSymbolName(RTN_Name(rtn), UNDECORATION_NAME_ONLY);
            if (!EsRutinaPropia(name))
                continue;

            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
                 InstAnalizer(ins);
            }
            RTN_InsertCall(rtn, IPOINT_BEFORE,
                           (AFUNPTR)RutinaEjecutada,
                           IARG_PTR, new std::string(name),
                           IARG_END);
            RTN_Close(rtn);
        }
    }
}

// ---------------- Finalización ----------------

VOID Fini(INT32 code, VOID *v) {
    std::cout << "\n===== RESULTADOS =====" << std::endl;
    std::cout << "ADD escalares: " << counterScalar.load() << std::endl;
    std::cout << "ADD SIMD:       " << counterSIMD.load() << std::endl;
    std::cout << "=======================" << std::endl;
}

// ---------------- Main ----------------

int main(int argc, char *argv[]) {
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        std::cerr << "Error inicializando PIN" << std::endl;
        return 1;
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}

