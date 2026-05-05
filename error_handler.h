#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdbool.h>

// סוגי השלבים בהם יכולה לקרות שגיאה
typedef enum {
    PHASE_LEXICAL,
    PHASE_SYNTAX,
    PHASE_SEMANTIC,
    PHASE_CODEGEN
} ErrorPhase;

// חתימות הפונקציות לניהול השגיאות
void initErrorHandler(void);
void reportError(ErrorPhase phase, int line, const char* format, ...);
bool hasErrors(void);
void printAllErrors(void);
void freeErrorHandler(void);

#endif // ERROR_HANDLER_H