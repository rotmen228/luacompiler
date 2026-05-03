#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "lexerH.h"
#include "semantic.h"

// Entry point — takes the AST root and the global symbol table,
// writes the fully translated C source to output.c
typedef struct {
    char*  data;
    int    length;
    int    capacity;
} OutputBuffer;

void generateCode(ASTNode* root, SymbolTable* globalTable);

#endif // CODEGEN_H
