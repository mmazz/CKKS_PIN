#include "pin.H"
#include <iostream>
#include <atomic>
#include <cstring>

std::atomic<uint64_t> counterScalar{0};
std::atomic<uint64_t> counterSIMD{0};
std::atomic<bool> flipped_once{false};
bool active_profiling = false;

// ---------------- Clasificación ----------------
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

// ---------------- Bit flip ----------------
VOID InjectFlipBit16_Result(ADDRINT ip, CONTEXT *ctxt, REG reg)
{
    // Evitar múltiples inyecciones
    if (flipped_once.exchange(true))
        return;

    const UINT32 bit = 16;
    if (REG_is_gr32(reg))
        reg = REG_FullRegName(reg); // convertir EBX->RBX, EAX->RAX...

    if (REG_is_xmm(reg) || REG_is_ymm(reg)) {
        UINT8 raw[32] = {0};
        PIN_GetContextRegval(ctxt, reg, raw);
        UINT32 byte_idx = bit / 8;
        UINT32 bit_in_byte = bit % 8;
        raw[byte_idx] ^= (UINT8)(1U << bit_in_byte);
        PIN_SetContextRegval(ctxt, reg, raw);
        std::cout << "[PIN] Flip bit16 in " << REG_StringShort(reg)
                  << " at " << std::hex << ip << std::dec << std::endl;
    } else if (REG_is_gr64(reg)) {
        ADDRINT val = PIN_GetContextReg(ctxt, reg);
        val ^= ((ADDRINT)1 << bit);
        PIN_SetContextReg(ctxt, reg, val);
        std::cout << "[PIN] Flip bit16 in " << REG_StringShort(reg)
                  << " at " << std::hex << ip << std::dec << std::endl;
    } else {
        std::cout << "[PIN] Skip unsupported reg " << REG_StringShort(reg)
                  << std::endl;
    }
}

// ---------------- Instrumentación ----------------
VOID InstAnalizer(INS ins)
{
    UINT32 opcode = INS_Opcode(ins);
    if (!(ScalarArithmetic(opcode) || SIMDArithmetic(opcode)))
        return;

    if (INS_MaxNumWRegs(ins) <= 0)
        return;

    REG destReg = INS_RegW(ins, 0);
    if (REG_is_gr32(destReg))
        destReg = REG_FullRegName(destReg);

    if (!REG_valid(destReg))
        return;

    // Insertamos AFTER (modifica resultado)
    INS_InsertPredicatedCall(
        ins, IPOINT_AFTER,
        (AFUNPTR)InjectFlipBit16_Result,
        IARG_INST_PTR,
        IARG_CONTEXT,
        IARG_UINT32, destReg,
        IARG_END);
}

// ---------------- Imagen ----------------
VOID ImageLoad(IMG img, VOID *v)
{
    if (!IMG_IsMainExecutable(img))
        return;

    RTN startRtn = RTN_FindByName(img, "start_profiling_marker");
    if (RTN_Valid(startRtn)) {
        RTN_Open(startRtn);
        RTN_InsertCall(startRtn, IPOINT_BEFORE, (AFUNPTR)+[] { active_profiling = true; }, IARG_END);
        RTN_Close(startRtn);
    }

    RTN stopRtn = RTN_FindByName(img, "stop_profiling_marker");
    if (RTN_Valid(stopRtn)) {
        RTN_Open(stopRtn);
        RTN_InsertCall(stopRtn, IPOINT_BEFORE, (AFUNPTR)+[] { active_profiling = false; }, IARG_END);
        RTN_Close(stopRtn);
    }

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            RTN_Open(rtn);
            for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
                InstAnalizer(ins);
            RTN_Close(rtn);
        }
    }
}

VOID Fini(INT32 code, VOID *v)
{
    std::cout << "\n===== RESULTADOS =====\n";
    std::cout << "ADD escalares: " << counterScalar.load() << "\n";
    std::cout << "ADD SIMD:       " << counterSIMD.load() << "\n";
    std::cout << "=======================\n";
}

int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if (PIN_Init(argc, argv)) {
        std::cerr << "Error inicializando PIN\n";
        return 1;
    }

    IMG_AddInstrumentFunction(ImageLoad, 0);
    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}

