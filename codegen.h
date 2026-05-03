#ifndef CODEGEN_H
#define CODEGEN_H
 
#include "ast.h"
#include "semantic.h"
 
// Entry point — takes the AST root, the global symbol table,
// and an output filename (e.g. "test_simple_output.c").
void generateCode(ASTNode* root, SymbolTable* globalTable, const char* outputFilename);
 
#endif // CODEGEN_H
 