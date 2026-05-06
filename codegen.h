#ifndef CODEGEN_H
#define CODEGEN_H
 
#include "ast.h"
#include "semantic.h"

typedef struct {
    char* data;
    int   length;
    int   capacity;
} OutputBuffer;

void generateCode(ASTNode* root, SymbolTable* globalTable, const char* outputFilename);

#endif
 