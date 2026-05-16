#ifndef AST_H
#define AST_H
#include "lexerH.h"

//defines all possible types of nodes in our Abstract Syntax Tree
typedef enum {
    AST_PROGRAM,
    AST_BLOCK,
    AST_ASSIGNMENT,
    AST_LOCAL_ASSIGN,
    AST_IF,
    AST_WHILE,
    AST_REPEAT,
    AST_FOR,
    AST_FUNCTION_DECL,
    AST_FUNCTION_CALL,
    AST_RETURN,
    AST_BINOP, //binary operation (e.g., a + b, x == y)
    AST_UNOP, //unary operation (e.g., -a, not b)
    AST_IDENTIFIER,
    AST_NUMBER,
    AST_STRING,
    AST_NIL
} ASTNodeType;

//the core structure for a node in the Abstract Syntax Tree
typedef struct ASTNode {
    ASTNodeType type;
    Token token;
    struct ASTNode** children; //dynamic array of pointers to child nodes
    int childCount; //current number of children attached
    int childCapacity; //max capacity before we need to reallocate memory
} ASTNode;

//holds the internal state of the parser as it processes tokens
typedef struct {
    TokenList* list;
    int current;
} Parser;

//functions correspond to the language's grammar rules.
ASTNode* parseStatement(Parser* p);
ASTNode* parseExpression(Parser* p);
ASTNode* parseIf(Parser* p);
ASTNode* parseWhile(Parser* p);
ASTNode* parseRepeat(Parser* p);
ASTNode* parseFor(Parser* p);
ASTNode* parseFunctionDef(Parser* p);
ASTNode* parseReturn(Parser* p);
ASTNode* parseLocal(Parser* p);
ASTNode* parseAssignOrCall(Parser* p);
ASTNode* parseBlock(Parser* p);


//helpers
ASTNode* createNode(ASTNodeType type, Token token);
void addChild(ASTNode* parent, ASTNode* child);
void freeAST(ASTNode* root);
void printAST(ASTNode* node, int depth);


ASTNode* runParser(TokenList* tokens);


#endif