#ifndef CODEGEN_H
#define CODEGEN_H
 
#include "ast.h"
#include "semantic.h"
 
void generateCode(ASTNode* root, SymbolTable* globalTable, const char* outputFilename);
 
#endif
 