#ifndef AST_H
#define AST_H

#include "lexerH.h" // אנחנו צריכים את מבנה האסימון (Token) שהגדרנו קודם

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
    
    AST_BINOP,
    AST_UNOP,
    
    AST_IDENTIFIER,
    AST_NUMBER,
    AST_STRING,
    AST_NIL
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    Token token;
    struct ASTNode** children;   
    int childCount;
    int childCapacity;
} ASTNode;

typedef struct {
    TokenList* list;
    int current;
} Parser;

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
ASTNode* createNode(ASTNodeType type, Token token);
void addChild(ASTNode* parent, ASTNode* child);
void freeAST(ASTNode* root);
void printAST(ASTNode* node, int depth);
ASTNode* runParser(TokenList* tokens);

#endif