/**
 * @brief CF binary module basic functions declaration file
 */

#ifndef CF_MODULE_H_
#define CF_MODULE_H_

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Instruction header representation enumeration
typedef enum __CfOpcode {
    // system instructions
    CF_OPCODE_UNREACHABLE,    ///< unreachable instruction, calls panic
    CF_OPCODE_SYSCALL,        ///< function by pre-defined index from without of sandbox calling function

    // i64 common instructions
    CF_OPCODE_I64_ADD,        ///< 64-bit integer addition
    CF_OPCODE_I64_SUB,        ///< 64-bit integer substraction
    CF_OPCODE_I64_SHL,        ///< 64-bit integer left shifting instruction

    // i64 signed/unsigned instructions
    CF_OPCODE_I64_MUL_S,      ///< signed   i64 multiplication
    CF_OPCODE_I64_MUL_U,      ///< unsigned i64 multiplication
    CF_OPCODE_I64_DIV_S,      ///< signed   i64 division
    CF_OPCODE_I64_DIV_U,      ///< unsigned i64 division
    CF_OPCODE_I64_SHR_S,      ///< signed   i64 left shifing instruction
    CF_OPCODE_I64_SHR_U,      ///< unsigned i64 left shifting instruction
    CF_OPCODE_I64_FROM_F64_S, ///< signed   i64 from f64 truncation function
    CF_OPCODE_I64_FROM_F64_U, ///< unsigned i64 from f64 truncation function

    // f64 arithmetical instructions
    CF_OPCODE_F64_ADD,        ///< f64 addition instruction
    CF_OPCODE_F64_SUB,        ///< f64 substraction instruction
    CF_OPCODE_F64_MUL,        ///< f64 multiplication instruction
    CF_OPCODE_F64_DIV,        ///< f64 division instruction
    CF_OPCODE_F64_FROM_I64_S, ///< signed   i64 into f64 conversion instruction
    CF_OPCODE_F64_FROM_I64_U, ///< unsigned i64 into f64 conversion instruction

    // common 64-bit instructions
    CF_OPCODE_R64_PUSH,       ///< 64-bit literal pushing function
    CF_OPCODE_R64_POP,        ///< 64-bit literal popping function
} CfOpcode;

/// @brief CF compiled module represetnation structure
typedef struct __CfModule {
    void    *code;        ///< module bytecode
    size_t   codeLength;  ///< module bytecode length
} CfModule;

/// @brief module reading status
typedef enum __CfModuleReadStatus {
    CF_MODULE_READ_STATUS_OK,                   ///< succeeded
    CF_MODULE_READ_STATUS_INTERNAL_ERROR,       ///< some internal error occured
    CF_MODULE_READ_STATUS_UNEXPECTED_FILE_END,  ///< unexpected end of file
    CF_MODULE_READ_STATUS_INVALID_MODULE_MAGIC, ///< invalid module magic number
    CF_MODULE_READ_STATUS_CODE_INVALID_HASH,    ///< invalid module code hash
} CfModuleReadStatus;

/**
 * @brief module from file reading function
 * 
 * @param[in] file   file to read module from, should allow "rb" access
 * @param[in] module module to write to file (non-null)
 * 
 * @return module reading status
 */
CfModuleReadStatus cfModuleRead( FILE *file, CfModule *dst );

/// @brief module to file writing status
typedef enum __CfModuleWriteStatus {
    CF_MODULE_WRITE_STATUS_OK,          ///< succeeded
    CF_MODULE_WRITE_STATUS_WRITE_ERROR, ///< writing to file operation failed
} CfModuleWriteStatus;

/**
 * @brief module to file writing function
 * 
 * @param[in]  module module to dump to file (non-null)
 * @param[out] file   destination file, should allow "wb" access
 * 
 * @return operation status
 */
CfModuleWriteStatus cfModuleWrite( const CfModule *module, FILE *dst );

/**
 * @brief module destructor
 * 
 * @param[in] module module to destroy
 */
void cfModuleDtor( CfModule *module );


/**
 * @brief module read status corresponding string getting function
 * 
 * @param[in] status module read status
 * 
 * @return corresponding string. In case of invalid status returns "<invalid>"
 */
const char * cfModuleReadStatusStr( CfModuleReadStatus status );

/**
 * @brief module write status corresponding string getting function
 * 
 * @param[in] status module writing status
 * 
 * @return corresponding string. In case of invalid status returns "<invalid>"
 */
const char * cfModuleWriteStatusStr( CfModuleWriteStatus status );

#ifdef __cplusplus
}
#endif

#endif // !defined(CF_MODULE_H_)

// cf_module.h
