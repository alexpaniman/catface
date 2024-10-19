#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>

#include <cf_vm.h>
#include <cf_stack.h>

/// @brief VM context representation structure
typedef struct __CfVm {
    uint8_t *         memory;                  ///< operative memory
    size_t            memorySize;              ///< current memory size (1 MB, actually)

    const CfModule  * module;                  ///< executed module
    const CfSandbox * sandbox;                 ///< execution environment (sandbox, actually)

    // registers
    CfRegisters       registers;               ///< user visible register
    const uint8_t   * instructionCounter;      ///< next instruction to execute pointer
    const uint8_t   * instructionCounterBegin; ///< pointer to first instruction
    const uint8_t   * instructionCounterEnd;   ///< pointer to first byte AFTER last instruciton

    // stascks
    CfStack           operandStack;            ///< function operand stack
    CfStack           callStack;               ///< call stack (contains previous instructionCounter's)

    /// panic-related fields
    CfTermInfo        termInfo;                ///< info to give to user if panic occured
    jmp_buf           panicJumpBuffer;         ///< to panic handler jump buffer
} CfVm;

/**
 * @brief execution termination function
 * 
 * @param[in,out] self   VM pointer
 * @param[in]     reason termination reason
 */
void cfVmTerminate( CfVm *self, const CfTermReason reason ) {
    assert(self != NULL);

    // fill standard termInfo fields
    self->termInfo.reason = reason;
    self->termInfo.offset = self->instructionCounter - (const uint8_t *)self->module->code;

    // jump to termination handler
    longjmp(self->panicJumpBuffer, 1);
} // cfVmTerminate

/**
 * @brief value by instruction counter popping function
 * 
 * @param[in,out] self  virtual machine to perform operation in
 * @param[out]    dst   operation destination
 * @param[in]     count count of bytes to read from instruction stream
 */
void cfVmRead( CfVm *const self, void *const dst, const size_t count ) {
    if (self->instructionCounterEnd - self->instructionCounter < count)
        cfVmTerminate(self, CF_TERM_REASON_UNEXPECTED_CODE_END);
    memcpy(dst, self->instructionCounter, count);
    self->instructionCounter += count;
} // cfVmRead

/**
 * @brief value to operand stack pushing function
 * 
 * @param[in,out] self virtual machine to perform action in
 * @param[out]    src  operand source (non-null, 4 bytes readable)
 */
void cfVmPushOperand( CfVm *const self, const void *const src ) {
    if (CF_STACK_OK != cfStackPush(&self->operandStack, src))
        cfVmTerminate(self, CF_TERM_REASON_INTERNAL_ERROR);
} // cfVmPushOperand

/**
 * @brief value from operand stack popping function
 * 
 * @param[in,out] self virtual machine to perform action in
 * @param[out]    dst  popping destination destination (non-null, 4 bytes writable)
 */
void cfVmPopOperand( CfVm *const self, void *const dst ) {
    switch (cfStackPop(&self->operandStack, dst)) {
    case CF_STACK_INTERNAL_ERROR : cfVmTerminate(self, CF_TERM_REASON_INTERNAL_ERROR);
    case CF_STACK_NO_VALUES      : cfVmTerminate(self, CF_TERM_REASON_NO_OPERANDS);
    case CF_STACK_OK             : break;
    }
} // cfVmPopOperand

/**
 * @brief jumping to certain point function
 * 
 * @param[in,out] self  virtual machine perform operation in
 * @param[in]     point point to jump to
 */
void cfVmJump( CfVm *const self, const uint32_t point ) {
    self->instructionCounter = self->instructionCounterBegin + point;

    // perform bound check
    if (self->instructionCounter >= self->instructionCounterEnd)
        cfVmTerminate(self, CF_TERM_REASON_INVALID_IC);
} // cfVmJump

/**
 * @brief generic conditional jump instruction implementation
 * 
 * @param[in,out] self      vm to perform action in
 * @param[in]     condition condition to 
 */
void cfVmGenericConditionalJump( CfVm *const self, const bool condition ) {
    uint32_t point;
    cfVmRead(self, &point, sizeof(point));
    if (condition)
        cfVmJump(self, point);
} // cfVmGenericConditionalJump

/**
 * @brief instruction counter to call stack pushing function
 * 
 * @param[in,out] self virtual machine to perform operation in
 */
void cfVmPushIC( CfVm *const self ) {
    if (CF_STACK_OK != cfStackPush(&self->callStack, &self->instructionCounter))
        cfVmTerminate(self, CF_TERM_REASON_INTERNAL_ERROR);
} // cfVmPushCall

/**
 * @brief instruction counter from call stack popping function
 * 
 * @param[in,out] self virtual machine to perform operation in
 */
void cfVmPopIC( CfVm *const self ) {
    switch (cfStackPop(&self->callStack, &self->instructionCounter)) {
    case CF_STACK_INTERNAL_ERROR : cfVmTerminate(self, CF_TERM_REASON_INTERNAL_ERROR);
    case CF_STACK_NO_VALUES      : cfVmTerminate(self, CF_TERM_REASON_CALL_STACK_UNDERFLOW);
    case CF_STACK_OK             : break;
    }
} // cfVmPopIC

/**
 * @brief value to register writing function
 * 
 * @param[in,out] self  virtual machine to perform operation in pointer
 * @param[in]     reg   register to write index
 * @param[in]     value value intended to write to register
 */
void cfVmWriteRegister( CfVm *const self, const uint32_t reg, const uint32_t value ) {
    if (reg >= CF_REGISTER_COUNT) {
        self->termInfo.unknownRegister = reg;
        cfVmTerminate(self, CF_TERM_REASON_UNKNOWN_REGISTER);
    }

    if (reg >= 2)
        self->registers.indexed[reg] = value;
} // cfVmWriteRegister

/**
 * @brief value to register writing function
 * 
 * @param[in,out] self virtual machine to perform operation in
 * @param[in]     reg  index of register to read
 * 
 * @return value from register by **reg** index
 */
uint32_t cfVmReadRegister( CfVm *const self, const uint32_t reg ) {
    if (reg >= CF_REGISTER_COUNT) {
        self->termInfo.unknownRegister = reg;
        cfVmTerminate(self, CF_TERM_REASON_UNKNOWN_REGISTER);
    }
    return self->registers.indexed[reg];
} // cfVmReadRegister

/**
 * @brief VM execution starting function
 * 
 * @param[in] vm reference of VM to start execution in
 */
void cfVmStart( CfVm *const self ) {

// macro definition, because there's no way to abstract type
#define GENERIC_BINARY_OPERATION(ty, operation) \
    {                                           \
        ty lhs, rhs;                            \
        cfVmPopOperand(self, &rhs);             \
        cfVmPopOperand(self, &lhs);             \
        lhs = lhs operation rhs;                \
        cfVmPushOperand(self, &lhs);            \
        break;                                  \
    }

#define GENERIC_COMPARISON(ty)                     \
    {                                              \
        ty lhs, rhs;                               \
        cfVmPopOperand(self, &rhs);                \
        cfVmPopOperand(self, &lhs);                \
        self->registers.fl.cmpIsEq = (lhs == rhs); \
        self->registers.fl.cmpIsLt = (lhs  < rhs); \
        break;                                     \
    }
#define GENERIC_CONVERSION(src, dst) \
    {                                \
        union {                      \
            src s;                   \
            dst d;                   \
        } sd;                        \
        cfVmPopOperand(self, &sd);   \
        sd.d = (dst)sd.s;            \
        cfVmPushOperand(self, &sd);  \
        break;                       \
    }

    // start infinite execution loop
    for (;;) {
        uint8_t opcode = *(self->instructionCounter++);

        switch ((CfOpcode)opcode) {
        case CF_OPCODE_UNREACHABLE: {
            cfVmTerminate(self, CF_TERM_REASON_UNREACHABLE);
        }

        case CF_OPCODE_SYSCALL: {
            uint32_t index;
            cfVmRead(self, &index, sizeof(index));

            /// TODO: remove this sh*tcode then import tables will be added.
            switch (index) {
            // readFloat64
            case 0: {
                float value = 0.304780;
                value = self->sandbox->readFloat64(self->sandbox->userContext);
                cfVmPushOperand(self, &value);
                break;
            }

            // writeFloat64
            case 1: {
                float argument;
                cfVmPopOperand(self, &argument);
                self->sandbox->writeFloat64(self->sandbox->userContext, argument);
                break;
            }

            default: {
                self->termInfo.unknownSystemCall = index;
                cfVmTerminate(self, CF_TERM_REASON_UNKNOWN_SYSTEM_CALL);
            }
            }
            break;
        }

        case CF_OPCODE_HALT: {
            cfVmTerminate(self, CF_TERM_REASON_HALT);
            break;
        }

        // set of generic binary operations
        case CF_OPCODE_ADD : GENERIC_BINARY_OPERATION(uint32_t,  +)
        case CF_OPCODE_SUB : GENERIC_BINARY_OPERATION(uint32_t,  -)
        case CF_OPCODE_SHL : GENERIC_BINARY_OPERATION(uint32_t, <<)
        case CF_OPCODE_SHR : GENERIC_BINARY_OPERATION(uint32_t, >>)
        case CF_OPCODE_SAR : GENERIC_BINARY_OPERATION( int32_t, >>)
        case CF_OPCODE_IMUL: GENERIC_BINARY_OPERATION( int32_t,  *)
        case CF_OPCODE_MUL : GENERIC_BINARY_OPERATION(uint32_t,  *)
        case CF_OPCODE_IDIV: GENERIC_BINARY_OPERATION( int32_t,  /)
        case CF_OPCODE_DIV : GENERIC_BINARY_OPERATION(uint32_t,  /)
        case CF_OPCODE_FADD: GENERIC_BINARY_OPERATION(   float,  +)
        case CF_OPCODE_FSUB: GENERIC_BINARY_OPERATION(   float,  -)
        case CF_OPCODE_FMUL: GENERIC_BINARY_OPERATION(   float,  *)
        case CF_OPCODE_FDIV: GENERIC_BINARY_OPERATION(   float,  /)

        // set of generic comparisons
        case CF_OPCODE_CMP : GENERIC_COMPARISON(uint32_t)
        case CF_OPCODE_ICMP: GENERIC_COMPARISON( int32_t)
        case CF_OPCODE_FCMP: GENERIC_COMPARISON(   float)

        // set of generic conversions
        case CF_OPCODE_FTOI: GENERIC_CONVERSION(float, int32_t)
        case CF_OPCODE_ITOF: GENERIC_CONVERSION(int32_t, float)

        case CF_OPCODE_JMP: cfVmGenericConditionalJump(self,
            true
        ); break;

        case CF_OPCODE_JLE: cfVmGenericConditionalJump(self,
            self->registers.fl.cmpIsLt || self->registers.fl.cmpIsEq
        ); break;

        case CF_OPCODE_JL : cfVmGenericConditionalJump(self,
            self->registers.fl.cmpIsLt
        ); break;

        case CF_OPCODE_JGE: cfVmGenericConditionalJump(self,
            !self->registers.fl.cmpIsLt
        ); break;

        case CF_OPCODE_JG : cfVmGenericConditionalJump(self,
            !self->registers.fl.cmpIsLt && !self->registers.fl.cmpIsEq
        ); break;

        case CF_OPCODE_JE : cfVmGenericConditionalJump(self,
            self->registers.fl.cmpIsEq
        ); break;

        case CF_OPCODE_JNE: cfVmGenericConditionalJump(self,
            !self->registers.fl.cmpIsEq
        ); break;

        case CF_OPCODE_CALL: {
            uint32_t point;
            cfVmRead(self, &point, sizeof(point));
            cfVmPushIC(self);
            cfVmJump(self, point);
            break;
        }

        case CF_OPCODE_RET: {
            cfVmPopIC(self);
            break;
        }

        case CF_OPCODE_PUSH: {
            CfPushPopInfo info;
            cfVmRead(self, &info, sizeof(info));

            if (info.isMemoryAccess)
                cfVmTerminate(self, CF_TERM_REASON_INTERNAL_ERROR);

            uint32_t value = 0;
            if (info.doReadImmediate)
                cfVmRead(self, &value, sizeof(value));
            value += cfVmReadRegister(self, info.registerIndex);
            cfVmPushOperand(self, &value);
            break;
        }

        case CF_OPCODE_POP: {
            CfPushPopInfo info;
            cfVmRead(self, &info, sizeof(info));

            if (info.doReadImmediate || info.isMemoryAccess)
                cfVmTerminate(self, CF_TERM_REASON_INTERNAL_ERROR);

            uint32_t value;
            cfVmPopOperand(self, &value);
            cfVmWriteRegister(self, info.registerIndex, value);
            break;
        }

        default: {
            self->termInfo.unknownOpcode = opcode;
            cfVmTerminate(self, CF_TERM_REASON_UNKNOWN_OPCODE);
        }
        }
    }

#undef GENERIC_JUMP
#undef GENERIC_COMPARISON
#undef GENERIC_BINARY_OPERATION
#undef GENERIC_CONVERSION
} // cfVmStart

/**
 * @brief module execution function
 * 
 * @param[in] module  module to execute
 * @param[in] sandbox execution environment
 * 
 * @return true if execution started, false if not
 */
bool cfModuleExec( const CfModule *module, const CfSandbox *sandbox ) {
    assert(module != NULL);
    assert(sandbox != NULL);

    // validate sandbox
    assert(sandbox->initialize != NULL);
    assert(sandbox->terminate != NULL);
    assert(sandbox->readFloat64 != NULL);
    assert(sandbox->writeFloat64 != NULL);

    // perform minimal context setup
    CfVm vm = {
        .module = module,
        .sandbox = sandbox,
    };
    bool isOk = true;

    // setup jumpBuffer
    // note: after jumpBuffer setup it's ok to do cleanup and call panic.
    int jmp = setjmp(vm.panicJumpBuffer);
    if (jmp) {
        sandbox->terminate(sandbox->userContext, &vm.termInfo);
        // then go to cleanup
        goto cfModuleExec__cleanup;
    }

    // allocate memory
    vm.memorySize = 1 << 20;
    vm.memory = (uint8_t *)calloc(vm.memorySize, 1);
    vm.callStack = cfStackCtor(sizeof(uint8_t *));
    vm.operandStack = cfStackCtor(sizeof(uint32_t));

    // here VM is not even initialized
    if (vm.memory == NULL || vm.callStack == NULL || vm.operandStack == NULL) {
        isOk = false;
        goto cfModuleExec__cleanup;
    }

    vm.instructionCounterBegin = (const uint8_t *)vm.module->code;
    vm.instructionCounterEnd   = (const uint8_t *)vm.module->code + vm.module->codeLength;

    vm.instructionCounter = vm.instructionCounterBegin;

    // try to initialize sandbox
    {
        CfExecContext execContext = {
            .memory = vm.memory,
            .memorySize = vm.memorySize,
        };

        // finish if initialization failed
        if (!sandbox->initialize(sandbox->userContext, &execContext)) {
            isOk = false;
            goto cfModuleExec__cleanup;
        }
    }

    // start execution
    cfVmStart(&vm);

    // perform cleanup
cfModuleExec__cleanup:
    free(vm.memory);
    cfStackDtor(vm.callStack);
    cfStackDtor(vm.operandStack);
    return isOk;
} // cfModuleExec2

// cf_vm.c
