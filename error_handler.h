#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdbool.h>

//error types
typedef enum {
    PHASE_LEXICAL,
    PHASE_SYNTAX,
    PHASE_SEMANTIC,
    PHASE_CODEGEN
} ErrorPhase;

//error obj
typedef struct {
    ErrorPhase phase;
    int line;
    char message[512];
} CompilerError;

void initErrorHandler(void);
void reportError(ErrorPhase phase, int line, const char* format, ...);
bool hasErrors(void);
void printAllErrors(void);
void freeErrorHandler(void);

#endif