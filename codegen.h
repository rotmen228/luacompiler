#ifndef CODEGEN_H
#define CODEGEN_H
 
#include "ast.h"
#include "semantic.h"

typedef struct {
    char* data;
    int   length;
    int   capacity;
} OutputBuffer;

char* generateCode(ASTNode* root, SymbolTable* globalTable);
static void generateBlock(ASTNode* node, int indent, SymbolTable* table);
static void generateAssign(ASTNode* node, int indent, SymbolTable* table);
static void generateLocalAssign(ASTNode* node, int indent, SymbolTable* table);
static void generateIf(ASTNode* node, int indent, SymbolTable* table);
static void generateLoop(ASTNode* node, int indent, SymbolTable* table);
static void generateFor(ASTNode* node, int indent, SymbolTable* table);
static void generateFunction(ASTNode* node, int indent, SymbolTable* table);
static void generateCall(ASTNode* node, int indent, SymbolTable* table);
static void generateReturn(ASTNode* node, int indent, SymbolTable* table);
static void generateExpression(ASTNode* node, SymbolTable* table);
static void generateGlobalDeclarations(SymbolTable* globalTable);
static void generateFunctions(ASTNode* root, SymbolTable* globalTable);

#endif