#include "pin.H"
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <iostream>
#include <vector>
#include <bitset>

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

static KNOB<BOOL> KnobFullRestore(
    KNOB_MODE_WRITEONCE, "pintool", "full_restore", "0",
    "Restaurar todos los coeficientes (1) o solo los modificados (0)");

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
static bool    flipApplied   = false;
static std::vector<UINT64> origCoeffs;

// Performance optimizations
static std::bitset<8192> modifiedCoeffs; // Track which coeffs were modified (max 8192)
static UINT64* coeffArray = nullptr;      // Direct pointer to coefficient array
static bool formatFuncAffectsAll = true; // Assume format affects all until we know better

// Logging optimizado
#define VLOG(msg) \
    do { if (KnobVerbose) std::cerr << msg << std::endl; } while (0)

// Logging ligero para operaciones frecuentes
#define VLOG_LIGHT(msg) \
    do { if (KnobVerbose.Value() ) std::cerr << msg << std::endl; } while (0)

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
    coeffArray = reinterpret_cast<UINT64*>(baseAddr); // Cache direct pointer
    VLOG("[DBG] objectAddr=0x" << std::hex << objectAddr
         << ", baseAddr=0x" << baseAddr << std::dec);
    return true;
}

// Optimized restore: only restore what was actually modified
inline void FastRestore() {
    if (KnobFullRestore.Value()) {
        // Full restore mode (slower but safer)
        for (UINT32 i = 0; i < KnobNumCoeffs; ++i) {
            coeffArray[i] = origCoeffs[i];
        }
        VLOG_LIGHT("[DBG] Full restore completed");
    } else {
        // Smart restore: only restore modified coefficients
        for (UINT32 i = 0; i < KnobNumCoeffs && i < modifiedCoeffs.size(); ++i) {
            if (modifiedCoeffs[i]) {
                coeffArray[i] = origCoeffs[i];
                modifiedCoeffs[i] = false;
                VLOG_LIGHT("[DBG] Restored coeff[" << i << "]");
            }
        }
    }
}

// Batch verification (only when verbose > 1)
inline void VerifyCleanState() {
    if (!KnobVerbose.Value()  ) return;

    bool allClean = true;
    for (UINT32 i = 0; i < KnobNumCoeffs; ++i) {
        if (coeffArray[i] != origCoeffs[i]) {
            VLOG("[ERROR] Coeff[" << i << "] = 0x" << std::hex << coeffArray[i]
                 << " != original 0x" << origCoeffs[i] << std::dec);
            allClean = false;
        }
    }
    if (allClean) {
        VLOG_LIGHT("[DBG] All coefficients verified clean");
    }
}

// ------------------------------------------------------------------------------------------------
// Callbacks
// ------------------------------------------------------------------------------------------------
inline void CallFormat() {
    VLOG_LIGHT("[DBG] CallFormat");
    if (fmt && KnobEnableEffect) {
        fmt((void*)objectAddr);

        // Mark potentially affected coefficients
        if (formatFuncAffectsAll) {
            // Conservative: assume all coefficients might be affected
            for (UINT32 i = 0; i < KnobNumCoeffs && i < modifiedCoeffs.size(); ++i) {
                modifiedCoeffs[i] = true;
            }
        } else {
            // If we know format only affects specific coeffs, mark only those
            // This would need profiling to determine
            modifiedCoeffs[curCoeff] = true;
        }
    }
}

VOID DoBitFlip() {
    // Early returns for performance
    if (!addressRead || !flipPending || flipApplied || curCoeff >= KnobNumCoeffs) {
        return;
    }

    VLOG_LIGHT("[DBG] DoBitFlip coeff=" << curCoeff << " bit=" << curBit);

    // PASO 1: Ensure target coefficient is clean (minimal verification)
    if (coeffArray[curCoeff] != origCoeffs[curCoeff]) {
        coeffArray[curCoeff] = origCoeffs[curCoeff];
        VLOG_LIGHT("[DBG] Restored target coeff to clean value");
    }

    // PASO 2: Format BEFORE
    if (KnobEnableEffect && fmt) {
        CallFormat();
    }

    // PASO 3: Apply bitflip
    if (KnobEnableEffect) {
        UINT64 mask = (1ULL << curBit);
        coeffArray[curCoeff] ^= mask;
        modifiedCoeffs[curCoeff] = true; // Mark as modified

        VLOG_LIGHT("[DBG] Flipped bit " << curBit << " of coeff " << curCoeff);
    }

    // PASO 4: Format AFTER
    if (KnobEnableEffect && fmt) {
        CallFormat();
    }

    flipApplied = true;
}

VOID RestoreAndAdvance() {
    if (!addressRead) return;

    VLOG_LIGHT("[DBG] RestoreAndAdvance coeff=" << curCoeff << " bit=" << curBit);

    // PASO 1: Optimized restore
    FastRestore();

    // PASO 2: Advance to next bit/coefficient
    if (curCoeff < KnobNumCoeffs) {
        curBit++;
        if (curBit >= 64) {
            curBit = 0;
            curCoeff++;
            if (curCoeff % 64 == 0) { // Log every 64 coefficients
                VLOG("[DBG] Advanced to coefficient: " << curCoeff);
            }
        }
    }

    // PASO 3: Prepare next flip
    if (curCoeff < KnobNumCoeffs) {
        flipPending = true;
        flipApplied = false;
    } else {
        flipPending = false;
        flipApplied = false;
        VLOG("[DBG] All coefficients processed");
    }

    // Optional verification (only when debugging)
    if (KnobVerbose.Value()  && (curBit == 0 || curCoeff >= KnobNumCoeffs)) {
        VerifyCleanState();
    }
}

// ------------------------------------------------------------------------------------------------
// Stubs en la app
// ------------------------------------------------------------------------------------------------
VOID OnSyncMarker() {
    RestoreAndAdvance();
}

VOID OnLabelHit() {
    VLOG("[DBG] OnLabelHit - initializing fault injection");

    // Read addresses if not done
    if (!addressRead && ReadAddresses()) {
        addressRead = true;
    }

    if (!addressRead) {
        std::cerr << "[ERROR] Failed to read addresses" << std::endl;
        return;
    }

    // Reserve space efficiently
    origCoeffs.clear();
    origCoeffs.reserve(KnobNumCoeffs.Value());
    modifiedCoeffs.reset(); // Clear all modification flags

    // Save original coefficients with minimal logging
    for (UINT32 i = 0; i < KnobNumCoeffs; ++i) {
        origCoeffs.push_back(coeffArray[i]);
    }

    if (KnobVerbose) {
        VLOG("[DBG] Saved " << KnobNumCoeffs.Value() << " original coefficients");
        if (KnobVerbose.Value()) {
            for (UINT32 i = 0; i < std::min((UINT32)8, KnobNumCoeffs.Value()); ++i) {
                VLOG("[DBG]   Orig[" << i << "] = 0x" << std::hex << origCoeffs[i] << std::dec);
            }
        }
    }

    // Initialize state
    curCoeff = 0;
    curBit = 0;
    flipPending = true;
    flipApplied = false;
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
                VLOG("[DBG] Captured format_func at 0x" << std::hex << RTN_Address(rtn) << std::dec);
            }
            else if (name == KnobTargetFunc.Value()) {
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
    VLOG("[DBG] Processed " << curCoeff << " coefficients, "
         << (curCoeff * 64 + curBit) << " total bits");

    if (addressRead && KnobVerbose.Value() ) {
        VLOG("[DBG] Final verification of first 8 coefficients:");
        for (UINT32 i = 0; i < std::min((UINT32)8, KnobNumCoeffs.Value()); ++i) {
            VLOG("[DBG]   Coeff[" << i << "] = 0x" << std::hex << coeffArray[i]
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
