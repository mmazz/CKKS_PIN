#include "pin.H"
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <vector>

// ------------------------------------------------------------------------------------------------
// Knobs
// ------------------------------------------------------------------------------------------------
static KNOB<std::string> KnobLabel(
    KNOB_MODE_WRITEONCE, "pintool", "label", "addr_label",
    "Función stub para disparar la lectura de direcciones");

static KNOB<std::string> KnobAddrFile(
    KNOB_MODE_WRITEONCE, "pintool", "addr_file", "target_address.txt",
    "Fichero con objectAddr y baseAddr");

static KNOB<std::string> KnobTargetFunc(
    KNOB_MODE_WRITEONCE, "pintool", "func", "foo",
    "Función donde hacer el bitflip");

static KNOB<UINT32> KnobInstrIndex(
    KNOB_MODE_WRITEONCE, "pintool", "instr_index", "4",
    "Índice de instrucción dentro de la función objetivo");

static KNOB<std::string> KnobFormatFunc(
    KNOB_MODE_WRITEONCE, "pintool", "format_func", "SwitchFormat",
    "Función de formato (NTT) para llamar antes/después del flip");

static KNOB<UINT32> KnobNumCoeffs(
    KNOB_MODE_WRITEONCE, "pintool", "num_coeffs", "1",
    "Número de coeficientes a procesar");

static KNOB<BOOL> KnobEnableEffect(
    KNOB_MODE_WRITEONCE, "pintool", "enable_effect", "1",
    "Habilitar flip real (1) o solo dry-run (0)");

static KNOB<BOOL> KnobVerbose(
    KNOB_MODE_WRITEONCE, "pintool", "verbose", "1",
    "Habilitar logs de depuración");

// ------------------------------------------------------------------------------------------------
// Types & Globals
// ------------------------------------------------------------------------------------------------
typedef void (*FormatFn)(void*);

static ADDRINT objectAddr    = 0;
static ADDRINT baseAddr      = 0;
static FormatFn fmt          = nullptr;
static bool    addressRead   = false;
static UINT32  curCoeff      = 0;
static UINT32  curBit        = 0;
static bool    flipPending   = false;
static bool    flipApplied   = false;  // NUEVO: track si ya aplicamos el flip
static std::vector<UINT64> origCoeffs;

// Macro de logging
#define VLOG(msg) \
    do { if (KnobVerbose) std::cerr << msg << std::endl; } while (0)

// ------------------------------------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------------------------------------
bool ReadAddresses() {
    int fd = open(KnobAddrFile.Value().c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[ERROR] No pude abrir: " << KnobAddrFile.Value() << std::endl;
        return false;
    }
    char buf[128] = {};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) {
        std::cerr << "[ERROR] Lectura vacía en " << KnobAddrFile.Value() << std::endl;
        return false;
    }
    unsigned long long obj=0, base=0;
    if (sscanf(buf, "%llx %llx", &obj, &base) != 2) {
        std::cerr << "[ERROR] Formato inválido en " << KnobAddrFile.Value() << std::endl;
        return false;
    }
    objectAddr = (ADDRINT)obj;
    baseAddr   = (ADDRINT)base;
    VLOG("[DBG] objectAddr=0x" << std::hex << objectAddr
         << ", baseAddr=0x" << baseAddr << std::dec);
    return true;
}

// ------------------------------------------------------------------------------------------------
// Callbacks
// ------------------------------------------------------------------------------------------------
VOID CallFormat() {
    VLOG("[DBG] CallFormat called");
    if (fmt && KnobEnableEffect) {
        fmt((void*)objectAddr);
        VLOG("[DBG]   format function executed");
    }
}

VOID DoBitFlip() {
    VLOG("[DBG] DoBitFlip - curCoeff=" << curCoeff << " curBit=" << curBit
         << " flipPending=" << flipPending << " flipApplied=" << flipApplied);

    if (!addressRead) {
        VLOG("[DBG] No address read yet");
        return;
    }

    if (!flipPending) {
        VLOG("[DBG] No flip pending");
        return;
    }

    if (flipApplied) {
        VLOG("[DBG] Flip already applied this iteration");
        return;
    }

    if (curCoeff >= KnobNumCoeffs) {
        VLOG("[DBG] No more coefficients to process");
        return;
    }

    // PASO 0: VERIFICAR que todos los coeficientes estén limpios antes del flip
    VLOG("[DBG] === Pre-flip verification: ensuring all coefficients are clean ===");
    for (UINT32 i = 0; i < KnobNumCoeffs && i < origCoeffs.size(); ++i) {
        UINT64* ptr = reinterpret_cast<UINT64*>(baseAddr + i * sizeof(UINT64));
        if (*ptr != origCoeffs[i]) {
            VLOG("[WARN] Coeff[" << i << "] = 0x" << std::hex << *ptr
                 << " != original 0x" << origCoeffs[i] << ", restoring..." << std::dec);
            *ptr = origCoeffs[i];
        }
    }

    UINT64* targetPtr = reinterpret_cast<UINT64*>(baseAddr + curCoeff * sizeof(UINT64));

    // Verificar que tenemos el valor original guardado
    if (curCoeff >= origCoeffs.size()) {
        VLOG("[ERROR] curCoeff out of bounds for origCoeffs");
        return;
    }

    // PASO 1: Asegurar que el coeficiente objetivo tiene el valor limpio
    *targetPtr = origCoeffs[curCoeff];
    VLOG("[DBG] Ensured target coeff[" << curCoeff << "] has clean value: 0x"
         << std::hex << *targetPtr << std::dec);

    // PASO 2: Aplicar format_func BEFORE (si está habilitado)
    if (KnobEnableEffect && fmt) {
        CallFormat();
        VLOG("[DBG] Applied format BEFORE flip");
    }

    // PASO 3: Aplicar bitflip
    UINT64 mask = (1ULL << curBit);
    if (KnobEnableEffect) {
        UINT64 beforeFlip = *targetPtr;
        *targetPtr ^= mask;
        VLOG("[DBG] Applied bitflip - mask=0x" << std::hex << mask
             << " before=0x" << beforeFlip << " after=0x" << *targetPtr << std::dec);
    }

    // PASO 4: Aplicar format_func AFTER (si está habilitado)
    if (KnobEnableEffect && fmt) {
        CallFormat();
        VLOG("[DBG] Applied format AFTER flip");
    }

    flipApplied = true;
    VLOG("[DBG] Bitflip sequence completed for coeff=" << curCoeff << " bit=" << curBit);

    // PASO 5: Mostrar estado final después del flip
    VLOG("[DBG] === Post-flip state ===");
    for (UINT32 i = 0; i < std::min((UINT32)8, KnobNumCoeffs.Value()) && i < origCoeffs.size(); ++i) {
        UINT64* ptr = reinterpret_cast<UINT64*>(baseAddr + i * sizeof(UINT64));
        VLOG("[DBG] Coeff[" << i << "] = 0x" << std::hex << *ptr
             << (i == curCoeff ? " <-- MODIFIED" : "") << std::dec);
    }
}

VOID RestoreAndAdvance() {
    VLOG("[DBG] RestoreAndAdvance - curCoeff=" << curCoeff << " curBit=" << curBit);

    if (!addressRead) {
        VLOG("[DBG] No addresses read yet");
        return;
    }

    // PASO 1: RESTAURAR TODOS los coeficientes a sus valores originales
    VLOG("[DBG] Restoring ALL coefficients to original values");
    for (UINT32 i = 0; i < KnobNumCoeffs && i < origCoeffs.size(); ++i) {
        UINT64* ptr = reinterpret_cast<UINT64*>(baseAddr + i * sizeof(UINT64));
        UINT64 currentValue = *ptr;
        *ptr = origCoeffs[i];

        if (currentValue != origCoeffs[i]) {
            VLOG("[DBG]   Restored coeff[" << i << "] from 0x" << std::hex << currentValue
                 << " to 0x" << origCoeffs[i] << std::dec);
        }
    }

    // PASO 2: Avanzar al siguiente bit/coeficiente
    if (curCoeff < KnobNumCoeffs) {
        curBit++;
        if (curBit >= 64) {
            curBit = 0;
            curCoeff++;
            VLOG("[DBG] Advanced to next coefficient: " << curCoeff);
        }
    }

    // PASO 3: Preparar para el siguiente flip
    if (curCoeff < KnobNumCoeffs) {
        flipPending = true;
        flipApplied = false;
        VLOG("[DBG] Next flip prepared: coeff=" << curCoeff << " bit=" << curBit);
    } else {
        flipPending = false;
        flipApplied = false;
        VLOG("[DBG] All coefficients processed");
    }

    // Debug: verificar que todos los coeficientes están limpios
    VLOG("[DBG] === Verification: All coefficients should match originals ===");
    bool allClean = true;
    for (UINT32 i = 0; i < KnobNumCoeffs && i < origCoeffs.size(); ++i) {
        UINT64* ptr = reinterpret_cast<UINT64*>(baseAddr + i * sizeof(UINT64));
        UINT64 currentValue = *ptr;
        if (currentValue != origCoeffs[i]) {
            VLOG("[ERROR] Coeff[" << i << "] = 0x" << std::hex << currentValue
                 << " != original 0x" << origCoeffs[i] << std::dec);
            allClean = false;
        } else {
            VLOG("[DBG] Coeff[" << i << "] = 0x" << std::hex << currentValue << " ✓" << std::dec);
        }
    }

    if (allClean) {
        VLOG("[DBG] All coefficients successfully restored to original values");
    } else {
        VLOG("[ERROR] Some coefficients were not properly restored!");
    }
}

// ------------------------------------------------------------------------------------------------
// Stubs en la app
// ------------------------------------------------------------------------------------------------
VOID OnSyncMarker() {
    VLOG("[DBG] OnSyncMarker called");
    RestoreAndAdvance();
}

VOID OnLabelHit() {
    VLOG("[DBG] OnLabelHit - reading addresses and saving original coefficients");

    // Leer direcciones si no las hemos leído
    if (!addressRead && ReadAddresses()) {
        addressRead = true;
        VLOG("[DBG] Addresses successfully read");
    }

    if (!addressRead) {
        VLOG("[ERROR] Failed to read addresses");
        return;
    }

    // Limpiar y guardar valores originales
    origCoeffs.clear();
    origCoeffs.reserve(KnobNumCoeffs.Value());

    VLOG("[DBG] Saving original coefficients:");
    for (UINT32 i = 0; i < KnobNumCoeffs; ++i) {
        UINT64* ptr = reinterpret_cast<UINT64*>(baseAddr + i * sizeof(UINT64));
        UINT64 original = *ptr;
        origCoeffs.push_back(original);
        VLOG("[DBG]   Orig coeff[" << i << "] = 0x" << std::hex << original << std::dec);
    }

    // Inicializar estado para el primer flip
    curCoeff = 0;
    curBit = 0;
    flipPending = true;
    flipApplied = false;

    VLOG("[DBG] Initialized for fault injection - ready for first flip");
}

// ------------------------------------------------------------------------------------------------
// Instrumentación
// ------------------------------------------------------------------------------------------------
VOID ImageCallback(IMG img, VOID*) {
    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            std::string name = RTN_Name(rtn);

            if (name == KnobLabel.Value()) {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnLabelHit), IARG_END);
                RTN_Close(rtn);
                VLOG("[DBG] Instrumented label: " << name);
            }
            else if (name == KnobFormatFunc.Value()) {
                fmt = (FormatFn)RTN_Address(rtn);
                VLOG("[DBG] Captured format_func at 0x"
                     << std::hex << RTN_Address(rtn) << std::dec);
            }
            else if (name == KnobTargetFunc.Value()) {
                VLOG("[DBG] Instrumenting target func: " << name);
                RTN_Open(rtn);
                UINT32 idx = 0;
                for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins), idx++) {
                    if (idx == KnobInstrIndex.Value()) {
                        INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(DoBitFlip), IARG_END);
                        VLOG("[DBG] Instrumented function " << name << " at instruction " << idx);
                        break;
                    }
                }
                RTN_Close(rtn);
            }
            else if (name == "sync_marker") {
                RTN_Open(rtn);
                RTN_InsertCall(rtn, IPOINT_BEFORE, AFUNPTR(OnSyncMarker), IARG_END);
                RTN_Close(rtn);
                VLOG("[DBG] Instrumented sync_marker()");
            }
        }
    }
}

VOID Fini(INT32, VOID*) {
    VLOG("[DBG] === FINAL STATE ===");
    VLOG("[DBG] Last coefficient processed: " << curCoeff << ", bit: " << curBit);

    // Mostrar estado final de todos los coeficientes
    if (addressRead) {
        VLOG("[DBG] Final coefficient values:");
        for (UINT32 i = 0; i < KnobNumCoeffs && i < origCoeffs.size(); ++i) {
            UINT64* ptr = reinterpret_cast<UINT64*>(baseAddr + i * sizeof(UINT64));
            VLOG("[DBG]   Final coeff[" << i << "] = 0x" << std::hex << *ptr
                 << " (orig: 0x" << origCoeffs[i] << ")" << std::dec);
        }
    }
}

int main(int argc, char* argv[]) {
    if (PIN_Init(argc, argv)) return 1;
    PIN_InitSymbols();
    IMG_AddInstrumentFunction(ImageCallback, nullptr);
    PIN_AddFiniFunction(Fini, nullptr);
    PIN_StartProgram();
    return 0;
}
