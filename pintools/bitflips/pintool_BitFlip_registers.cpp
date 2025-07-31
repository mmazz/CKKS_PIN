#include "pin.H"
#include <iostream>
#include <fstream>
#include <string>

using namespace std;

// Knobs for configuration
KNOB<string> KnobTargetFunc(KNOB_MODE_WRITEONCE, "pintool",
    "target_func", "", "Target OpenFHE function name");

KNOB<UINT64> KnobTargetArithOp(KNOB_MODE_WRITEONCE, "pintool",
    "target_arith", "0", "N-th arithmetic operation within target function (0-based)");

KNOB<UINT32> KnobTargetBit(KNOB_MODE_WRITEONCE, "pintool",
    "target_bit", "0", "Bit position to flip (0-255)");

KNOB<string> KnobLogFile(KNOB_MODE_WRITEONCE, "pintool",
    "log", "fault_injection.log", "Log file path");

// Global variables
static bool inside_target_function = false;
static UINT64 arith_ops_in_function = 0;
static UINT64 target_arith_op_number = 0;
static UINT32 target_bit = 0;
static string target_function = "";
static ofstream logfile;
static bool fault_injected = false;

// Check if instruction is arithmetic
bool IsArithmeticInstruction(INS ins) {
    OPCODE opcode = INS_Opcode(ins);
    return (opcode == XED_ICLASS_ADD ||
            opcode == XED_ICLASS_SUB ||
            opcode == XED_ICLASS_MUL ||
            opcode == XED_ICLASS_IMUL ||
            opcode == XED_ICLASS_ADDSD ||
            opcode == XED_ICLASS_SUBSD ||
            opcode == XED_ICLASS_MULSD ||
            opcode == XED_ICLASS_ADDSS ||
            opcode == XED_ICLASS_SUBSS ||
            opcode == XED_ICLASS_MULSS);
}

// Get the destination register of an instruction
REG GetDestinationRegister(INS ins) {
    if (INS_OperandCount(ins) > 0 && INS_OperandIsReg(ins, 0)) {
        return INS_OperandReg(ins, 0);
    }
    return REG_INVALID();
}

// Flip bit in register
VOID FlipBitInRegister(CONTEXT *ctx, REG reg, UINT32 bit_pos, ADDRINT ip) {
    UINT8 reg_value[32];  // 256 bits should be enough for most registers

    if (REG_valid(reg)) {
        // Get register value
        PIN_GetContextRegval(ctx, reg, reg_value);

        // Calculate byte and bit position
        UINT32 byte_pos = bit_pos / 8;
        UINT32 bit_in_byte = bit_pos % 8;

        // Flip the specified bit
        if (byte_pos < 32) {
            reg_value[byte_pos] ^= (1 << bit_in_byte);
        }

        // Set register value back
        PIN_SetContextRegval(ctx, reg, reg_value);

        logfile << "FAULT INJECTED: Function=" << target_function
                << " ArithOp=" << arith_ops_in_function
                << " Bit=" << bit_pos
                << " Register=" << REG_StringShort(reg)
                << " IP=0x" << hex << ip << dec << endl;
        logfile.flush();

        fault_injected = true;
    }
}

// Count arithmetic operations
VOID CountArithOp() {
    if (inside_target_function) {
        arith_ops_in_function++;
    }
}

// Conditional bit flip after instruction execution
VOID ConditionalBitFlip(CONTEXT *ctx, ADDRINT ip, REG dest_reg) {
    if (inside_target_function &&
        arith_ops_in_function == target_arith_op_number &&
        !fault_injected) {

        FlipBitInRegister(ctx, dest_reg, target_bit, ip);
    }
}

// Function entry callback
VOID EnterTargetFunction(ADDRINT func_addr) {
    if (!inside_target_function) {  // Avoid nested calls
        inside_target_function = true;
        arith_ops_in_function = 0;
        fault_injected = false;

        logfile << "ENTER: " << target_function << " at 0x" << hex << func_addr << dec << endl;
        logfile.flush();
    }
}

// Function exit callback
VOID ExitTargetFunction(ADDRINT func_addr) {
    if (inside_target_function) {
        inside_target_function = false;

        logfile << "EXIT: " << target_function << " at 0x" << hex << func_addr << dec
                << " (Total arith ops: " << arith_ops_in_function << ")" << endl;
        logfile.flush();
    }
}

// Instruction instrumentation
VOID Instruction(INS ins, VOID *v) {
    if (IsArithmeticInstruction(ins)) {
        // Count arithmetic operations
        INS_InsertCall(ins, IPOINT_BEFORE,
                       (AFUNPTR)CountArithOp,
                       IARG_END);

        // Get destination register
        REG dest_reg = GetDestinationRegister(ins);

        if (REG_valid(dest_reg)) {
            // Insert fault injection after instruction execution
            INS_InsertCall(ins, IPOINT_AFTER,
                           (AFUNPTR)ConditionalBitFlip,
                           IARG_CONTEXT,
                           IARG_INST_PTR,
                           IARG_UINT32, dest_reg,
                           IARG_END);
        }
    }
}

// Image instrumentation
VOID Image(IMG img, VOID *v) {
    // Debug: Log all images being loaded
    logfile << "DEBUG: Loading image: " << IMG_Name(img) << endl;

    if (target_function.empty()) return;

    bool found_function = false;

    for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
        for (RTN rtn = SEC_RtnHead(sec); RTN_Valid(rtn); rtn = RTN_Next(rtn)) {
            string name = RTN_Name(rtn);

            // Debug: Log some function names to see what's available
            if (name.find("Encrypt") != string::npos || name.find("encrypt") != string::npos) {
                logfile << "DEBUG: Found encrypt-related function: " << name << endl;
            }

            if (name == target_function) {
                RTN_Open(rtn);

                // Insert function entry/exit callbacks
                RTN_InsertCall(rtn, IPOINT_BEFORE,
                               (AFUNPTR)EnterTargetFunction,
                               IARG_ADDRINT, RTN_Address(rtn),
                               IARG_END);

                RTN_InsertCall(rtn, IPOINT_AFTER,
                               (AFUNPTR)ExitTargetFunction,
                               IARG_ADDRINT, RTN_Address(rtn),
                               IARG_END);

                logfile << "INSTRUMENTED: Function " << name
                        << " at 0x" << hex << RTN_Address(rtn) << dec << endl;

                found_function = true;
                RTN_Close(rtn);
                break;
            }
        }
        if (found_function) break;
    }
}

// Cleanup on exit
VOID Fini(INT32 code, VOID *v) {
    logfile << "SUMMARY: Target=" << target_function
            << " TargetOp=" << target_arith_op_number
            << " TargetBit=" << target_bit
            << " FaultInjected=" << (fault_injected ? "YES" : "NO") << endl;
    logfile.close();
}

// Usage information
INT32 Usage() {
    cerr << "CKKS Fault Injection Pin Tool" << endl;
    cerr << "Usage: pin -t <tool> [options] -- <program>" << endl;
    cerr << "Options:" << endl;
    cerr << "  -target_func <name>     : Target function name" << endl;
    cerr << "  -target_arith <N>       : N-th arithmetic operation (0-based)" << endl;
    cerr << "  -target_bit <bit>       : Bit position to flip (0-255)" << endl;
    cerr << "  -log <path>             : Log file path" << endl;
    cerr << endl;
    cerr << "Example:" << endl;
    cerr << "  pin -t fault_tool.so -target_func Encrypt -target_arith 100 -target_bit 15 -- ./ckks_test" << endl;
    return -1;
}

// Main function
int main(int argc, char *argv[]) {
    PIN_InitSymbols();

    if (PIN_Init(argc, argv)) return Usage();

    // Get parameters
    target_function = KnobTargetFunc.Value();
    target_arith_op_number = KnobTargetArithOp.Value();
    target_bit = KnobTargetBit.Value();

    if (target_function.empty()) {
        cerr << "Error: target_func parameter is required" << endl;
        return Usage();
    }

    // Open log file
    logfile.open(KnobLogFile.Value().c_str());
    if (!logfile.is_open()) {
        cerr << "Error: Cannot open log file " << KnobLogFile.Value() << endl;
        return -1;
    }

    // Force immediate write to ensure file is created
    logfile << "CKKS Fault Injection Started" << endl;
    logfile << "Target Function: " << target_function << endl;
    logfile << "Target Arithmetic Operation: " << target_arith_op_number << endl;
    logfile << "Target Bit: " << target_bit << endl;
    logfile << "================================" << endl;
    logfile.flush();  // Force write to disk

    cerr << "Log file created: " << KnobLogFile.Value() << endl;  // Debug to stderr

    // Register callbacks
    IMG_AddInstrumentFunction(Image, 0);
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    // Start program
    PIN_StartProgram();

    return 0;
}
