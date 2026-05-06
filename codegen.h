#ifndef CODEGEN_H
#define CODEGEN_H
 
#include "ast.h"
#include "semantic.h"

typedef struct {
    char* data;
    int   length;
    int   capacity;
} OutputBuffer;

// השינוי: הפונקציה עכשיו מחזירה char* ולא מקבלת outputFilename
char* generateCode(ASTNode* root, SymbolTable* globalTable);

#endif