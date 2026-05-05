#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "error_handler.h"

// מבנה לשגיאה בודדת
typedef struct {
    ErrorPhase phase;
    int line;
    char message[512];
} CompilerError;

static CompilerError* errorList = NULL;
static int errorCount = 0;
static int errorCapacity = 0;

void initErrorHandler(void) {
    errorCount = 0;
    errorCapacity = 16;
    errorList = (CompilerError*)malloc(errorCapacity * sizeof(CompilerError));
}

// הפונקציה שאוספת את השגיאות למערך
void reportError(ErrorPhase phase, int line, const char* format, ...) {
    if (errorCount >= errorCapacity) {
        errorCapacity *= 2;
        errorList = (CompilerError*)realloc(errorList, errorCapacity * sizeof(CompilerError));
    }

    errorList[errorCount].phase = phase;
    errorList[errorCount].line = line;

    // עיבוד המחרוזת עם המשתנים (כמו printf)
    va_list args;
    va_start(args, format);
    vsnprintf(errorList[errorCount].message, sizeof(errorList[errorCount].message), format, args);
    va_end(args);

    errorCount++;
}

bool hasErrors(void) {
    return errorCount > 0;
}

// הפונקציה שמדפיסה את הדו"ח הסופי
void printAllErrors(void) {
    if (errorCount == 0) return;

    printf("\n=============================================================\n");
    printf("  COMPILATION FAILED: %d Error(s) Found\n", errorCount);
    printf("=============================================================\n");

    for (int i = 0; i < errorCount; i++) {
        const char* phaseName = "";
        switch (errorList[i].phase) {
            case PHASE_LEXICAL:  phaseName = "Lexical"; break;
            case PHASE_SYNTAX:   phaseName = "Syntax"; break;
            case PHASE_SEMANTIC: phaseName = "Semantic"; break;
            case PHASE_CODEGEN:  phaseName = "CodeGen"; break;
        }

        if (errorList[i].line > 0) {
            printf("[%s Error] Line %d: %s\n", phaseName, errorList[i].line, errorList[i].message);
        } else {
            printf("[%s Error]: %s\n", phaseName, errorList[i].message);
        }
    }
    printf("=============================================================\n\n");
}

void freeErrorHandler(void) {
    if (errorList) {
        free(errorList);
        errorList = NULL;
    }
    errorCount = 0;
    errorCapacity = 0;
}