#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "error_handler.h"


static CompilerError* errorList = NULL;
static int errorCount = 0;
static int errorCapacity = 0;
//prepares the dynamic array that will hold all compiler errors
void initErrorHandler(void) {
    errorCount = 0;
    errorCapacity = 16;
    errorList = (CompilerError*)malloc(errorCapacity * sizeof(CompilerError));
}
//captures an error, formats the custom message, and adds it to the list
//uses '...' to accept a variable number of arguments
void reportError(ErrorPhase phase, int line, const char* format, ...) {
    if (errorCount >= errorCapacity) {
        errorCapacity *= 2;
        errorList = (CompilerError*)realloc(errorList, errorCapacity * sizeof(CompilerError));
    }

    errorList[errorCount].phase = phase;
    errorList[errorCount].line = line;
    //va_list unpacks the '...' arguments passed into this function
    va_list args;
    va_start(args, format);
    //vsnprintf securely writes the formatted string into our structs message buffer
    vsnprintf(errorList[errorCount].message, sizeof(errorList[errorCount].message), format, args);
    va_end(args);

    errorCount++;
}

bool hasErrors(void) {
    return errorCount > 0;
}
//prints a nicely formatted summary of every error caught by the compiler
void printAllErrors(void) {
    if (errorCount == 0) return;

    printf("\n=============================================================\n");
    printf("  COMPILATION FAILED: %d Error(s) Found\n", errorCount);
    printf("=============================================================\n");

    for (int i = 0; i < errorCount; i++) {
        const char* phaseName = "";
        switch (errorList[i].phase) {
            case PHASE_LEXICAL: phaseName = "Lexical"; break;
            case PHASE_SYNTAX: phaseName = "Syntax"; break;
            case PHASE_SEMANTIC: phaseName = "Semantic"; break;
            case PHASE_CODEGEN: phaseName = "CodeGen"; break;
        }

        if (errorList[i].line > 0) {
            printf("[%s Error] Line %d: %s\n", phaseName, errorList[i].line, errorList[i].message);
        } else {
            printf("[%s Error]: %s\n", phaseName, errorList[i].message);
        }
    }
    printf("=============================================================\n\n");
}
//cleanup
void freeErrorHandler(void) {
    if (errorList) {
        free(errorList);
        errorList = NULL;
    }
    errorCount = 0;
    errorCapacity = 0;
}