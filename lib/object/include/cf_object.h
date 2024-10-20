/**
 * @brief object declaration file
 */

#ifndef CF_OBJECT_H_
#define CF_OBJECT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @brief maximal length of jump label supported
#define CF_LABEL_MAX 64

/// @brief label and link (references to certain code point with different semantics)
typedef struct __CfLabelAndLink {
    uint32_t sourceLine;     ///< line label declared at
    uint32_t codeOffset;     ///< offset link encodes
    char     label     [64]; ///< label
} CfLabel, CfLink;

/// @brief object (single .cfasm compilation result) represetnation structure
typedef struct __CfObject {
    const char * sourceFileName; ///< name of object source name
    size_t       codeLength;     ///< length of bytecode
    uint8_t    * code;           ///< bytecode itself
    size_t       linkCount;      ///< count of links in bytecode
    CfLink     * links;          ///< links itself
    size_t       labelCount;     ///< count of labels in bytecode
    CfLabel    * labels;         ///< labels itself
} CfObject;

/// @brief object from file reading status representation enumeration
typedef enum __CfObjectReadStatus {
    CF_OBJECT_READ_STATUS_OK,                   ///< succeeded
    CF_OBJECT_READ_STATUS_INTERNAL_ERROR,       ///< internal error occured
    CF_OBJECT_READ_STATUS_UNEXPECTED_FILE_END,  ///< reading from file failed
    CF_OBJECT_READ_STATUS_INVALID_OBJECT_MAGIC, ///< invalid object magic
} CfObjectReadStatus;

/**
 * @brief object from file reading function
 * 
 * @param[in]  file file to read (opened for binary reading)
 * @param[out] dst  reading destination
 * 
 * @return operation status
 */
CfObjectReadStatus cfObjectRead( FILE *file, CfObject *dst );

/// @brief object to file writing status representation enumeration
typedef enum __CfObjectWriteStatus {
    CF_OBJECT_WRITE_STATUS_OK,          ///< succeeded
    CF_OBJECT_WRITE_STATUS_WRITE_ERROR, ///< writing to file error
} CfObjectWriteStatus;

/**
 * @brief object to file writing function
 * 
 * @param[in] file file to read object from (opened for binary writing)
 * @param[in] src  object to write
 * 
 * @return operation status
 */
bool cfObjectWrite( FILE *file, const CfObject *src );

/**
 * @brief object data destructor
 * 
 * @param[in] object object to destroy pointer
 */
void cfObjectDtor( CfObject *object );

#ifdef __cplusplus
}
#endif

#endif // !defined(CF_OBJECT_H_)

// cf_object.h