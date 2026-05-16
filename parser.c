#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "lexerH.h"
#include "ast.h"
#include "error_handler.h"

// ==========================================
// helpers
// ==========================================
//make new AST node
ASTNode* createNode(ASTNodeType type, Token token) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) {
        printf("Memory allocation failed for ASTNode!\n");
        exit(1);
    }
    node->type = type;
    node->token = token;
    node->childCount = 0;
    node->childCapacity = 2;
    node->children = (ASTNode**)malloc(node->childCapacity * sizeof(ASTNode*));
    return node;
}
//add child to an existing node
void addChild(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;
    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity *= 2;
        parent->children = (ASTNode**)realloc(parent->children, parent->childCapacity * sizeof(ASTNode*));
    }
    parent->children[parent->childCount++] = child;
}
//recursivly frees the memory of the AST node
void freeAST(ASTNode* root) {
    if (!root) return;
    for (int i = 0; i < root->childCount; i++) {
        freeAST(root->children[i]);
    }
    free(root->children);
    free(root);
}
//peek at the current token, dont consume
static Token peek(Parser* p) {
    if (p->current >= p->list->count) {
        return p->list->tokens[p->list->count - 1]; // reutrn EOF token in the end of the list
    }
    return p->list->tokens[p->current];
}
//consume current token
static Token consume(Parser* p) {
    Token t = peek(p);
    p->current++;
    return t;
}
//check if current token matches a given token, if so, consume it. return bool
static bool match(Parser* p, TokenType expectedType) {
    if (peek(p).type == expectedType) {
        consume(p);
        return true;
    }
    return false;
}

//error in this state compremises the rest of the program, exit
static void parseError(Parser* p, const char* message) {
    Token t = peek(p);
    reportError(PHASE_SYNTAX, t.line, "%s (got '%s')", message, t.value ? t.value : "EOF");
    printAllErrors();
    exit(1);
}

// ==========================================
// main function for parser
// ==========================================
ASTNode* runParser(TokenList* tokens) {
    //initialize main parser object
    Parser p = {tokens, 0};
    //initialize root of AST as EOF token
    Token dummyToken = {TOKEN_EOF, NULL, 0};
    ASTNode* programNode = createNode(AST_PROGRAM, dummyToken);
    //add statments at root level in the AST
    while (peek(&p).type != TOKEN_EOF) {
        ASTNode* stmt = parseStatement(&p);
        if (stmt) {
            addChild(programNode, stmt);
        }
    }
    
    return programNode;
}

// ==========================================
// Recursive Descent
// ==========================================
ASTNode* parseStatement(Parser* p) {
    Token t = peek(p);
    //what type of AST node
    switch (t.type) {
        case TOKEN_KW_IF: return parseIf(p);
        case TOKEN_KW_WHILE: return parseWhile(p);
        case TOKEN_KW_FOR: return parseFor(p);
        case TOKEN_KW_FUNCTION: return parseFunctionDef(p);
        case TOKEN_KW_RETURN: return parseReturn(p);
        case TOKEN_KW_LOCAL: return parseLocal(p);
        case TOKEN_KW_REPEAT: return parseRepeat(p);
        case TOKEN_IDENTIFIER: return parseAssignOrCall(p);
        default:
            reportError(PHASE_SYNTAX, peek(p).line, "Unexpected token '%s'", peek(p).value);
            consume(p);
            return NULL;
    }
}

ASTNode* parseLocal(Parser* p) {
    consume(p); //local
    Token id = consume(p); //id
    //error if not expected token (ID)
    if (id.type != TOKEN_IDENTIFIER) {
        parseError(p, "Expected variable name after 'local'");
    }
    ASTNode* idNode = createNode(AST_IDENTIFIER, id);
    //there is assignment
    if (match(p, TOKEN_OP_ASSIGN)) {
        //make assign node and children
        ASTNode* valueNode = parseExpression(p);
        ASTNode* assignNode = createNode(AST_LOCAL_ASSIGN, (Token){TOKEN_OP_ASSIGN, "=", p->list->tokens[p->current - 1].line});
        addChild(assignNode, idNode);
        addChild(assignNode, valueNode);
        return assignNode;
    } else { //no assignment, is nil
        //make assign node and children
        ASTNode* nilNode = createNode(AST_NIL, (Token){TOKEN_KW_NIL, "nil", p->list->tokens[p->current - 1].line});
        ASTNode* assignNode = createNode(AST_LOCAL_ASSIGN, (Token){TOKEN_OP_ASSIGN, "=", p->list->tokens[p->current - 1].line});
        addChild(assignNode, idNode);
        addChild(assignNode, nilNode);
        return assignNode;
    }
}

ASTNode* parseReturn(Parser* p) {
    // 1. צרוך את האסימון return
    Token returnToken = consume(p); 
    
    ASTNode* valueNode = NULL;
    TokenType nextType = peek(p).type;
    
    // 2. בדוק אם האסימון הנוכחי הוא סוף בלוק (end, else, elseif, until, EOF)
    if (nextType == TOKEN_KW_END || nextType == TOKEN_KW_ELSE || nextType == TOKEN_KW_ELSEIF || nextType == TOKEN_KW_UNTIL || nextType == TOKEN_EOF) {
        // 2.1 צור צומת עץ מסוג NIL
        Token nilToken = {TOKEN_KW_NIL, "nil", returnToken.line};
        valueNode = createNode(AST_NIL, nilToken);
    } else {
        // 2.2 קרא לפונקציה parseExpression ושמור את התוצאה
        valueNode = parseExpression(p);
    }
    
    // 3. החזר צומת מסוג RETURN עם ילד אחד
    ASTNode* returnNode = createNode(AST_RETURN, returnToken);
    addChild(returnNode, valueNode);
    return returnNode;
}

ASTNode* parseAssignOrCall(Parser* p) {
    // 1. שמור את שם המשתנה/הפונקציה מהאסימון הנוכחי IDENTIFIER וצרוך אותו.
    Token idToken = consume(p);
    
    // 2. אם האסימון הנוכחי הוא '=' :
    if (peek(p).type == TOKEN_OP_ASSIGN) {
        // 2.1. צרוך את האסימון '='.
        consume(p); 
        
        // 2.2. קרא לפונקציה ()parseExpression ושמור את התוצאה בשם צומת_ערך.
        ASTNode* valueNode = parseExpression(p);
        
        // 2.3. החזר צומת עץ מסוג ASSIGN עם שני ילדים: שם המשתנה, ו-צומת_ערך.
        ASTNode* assignNode = createNode(AST_ASSIGNMENT, idToken);
        addChild(assignNode, createNode(AST_IDENTIFIER, idToken)); // ילד 1: המשתנה
        addChild(assignNode, valueNode);                           // ילד 2: הערך
        
        return assignNode;
    }
    
    // 3. אם האסימון הנוכחי הוא '(' :
    else if (peek(p).type == TOKEN_PUNC_LPAREN) {
        // 3.1. צרוך את האסימון '(' .
        consume(p); 
        
        // יצירת צומת ה-FUNCTION_CALL עם שם הפונקציה
        ASTNode* callNode = createNode(AST_FUNCTION_CALL, idToken);
        
        // 3.2. קרא לפונקציה ()parseFunctionArguments (לולאה שאוספת עד ה- ')' )
        while (peek(p).type != TOKEN_PUNC_RPAREN && peek(p).type != TOKEN_EOF) {
            addChild(callNode, parseExpression(p));
            // בולע פסיקים בין הארגומנטים אם יש
            if (peek(p).type == TOKEN_PUNC_COMMA) consume(p); 
        }
        
        // 3.3. צרוך את האסימון ')' .
        if (!match(p, TOKEN_PUNC_RPAREN)) {
            parseError(p, "Expected ')' after function arguments");
        }
        
        // 3.4. החזר צומת עץ מסוג FUNCTION_CALL שהילדים שלו הם הארגומנטים.
        return callNode;
    }
    
    // אחרת:
    else {
        // 3.1 (או 4). זרוק שגיאת תחביר
        parseError(p, "Expected '=' for assignment or '(' for function call");
        return NULL; // לא יקרה בפועל כי parseError עושה exit
    }
}

ASTNode* parseIf(Parser* p) {
    //consume the if or elseif token
    Token ifToken = consume(p); 
    //parse the condition and ensure then follows
    ASTNode* condition = parseExpression(p);
    if (!match(p, TOKEN_KW_THEN)) {
        parseError(p, "Expected 'then' after condition");
    }
    //parse the main body block
    ASTNode* body = parseBlock(p);
    ASTNode* elseNode = NULL;
    //handle if/elseif/else chain
    switch (peek(p).type) {
        case TOKEN_KW_ELSEIF:
            //recursively call parseIf. It will consume the elseif
            elseNode = parseIf(p); 
            break;
            
        case TOKEN_KW_ELSE:
            //consume the else token, then parse the final block
            consume(p); 
            elseNode = parseBlock(p);
            break;
            
        default:
            break;
    }
    //only the root if needs an end
    if (ifToken.type == TOKEN_KW_IF) {
        if (!match(p, TOKEN_KW_END)) {
            parseError(p, "Expected 'end' to close if statement");
        }
    }
    //construct the AST node
    ASTNode* node = createNode(AST_IF, ifToken);
    addChild(node, condition);
    addChild(node, body);
    //add the else/elseif branch if it exists, ifnot add a NIL so the if node always has 3 childern
    if (elseNode) {
        addChild(node, elseNode);
    } else {
        addChild(node, createNode(AST_NIL, (Token){TOKEN_KW_NIL, "nil", 0}));
    }
    return node;
}

ASTNode* parseWhile(Parser* p) {
    //consume while and the condition expression
    Token whileToken = consume(p);
    ASTNode* condition = parseExpression(p);
    //error if not exists
    if (!match(p, TOKEN_KW_DO)) parseError(p, "Expected 'do' after while condition");
    //consume bosy
    ASTNode* body = parseBlock(p);
    if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' to close while loop");
    //make AST node with children
    ASTNode* node = createNode(AST_WHILE, whileToken);
    addChild(node, condition);
    addChild(node, body);
    return node;
}

ASTNode* parseRepeat(Parser* p) {
    //consume repeat keyword and body
    Token repeatToken = consume(p);
    ASTNode* body = parseBlock(p);
    //error if not exists
    if (!match(p, TOKEN_KW_UNTIL)) parseError(p, "Expected 'until' after repeat block");
    
    //consume condition expression
    ASTNode* condition = parseExpression(p);
    //make AST node with children
    ASTNode* node = createNode(AST_REPEAT, repeatToken);
    addChild(node, body);
    addChild(node, condition);
    return node;
}

ASTNode* parseFor(Parser* p) {
    //consume the for token and the variable name
    Token forToken = consume(p);
    Token varName = consume(p);
    //error if not exits
    if (varName.type != TOKEN_IDENTIFIER) {
        parseError(p, "Expected variable name in for loop");
    }
    if (!match(p, TOKEN_OP_ASSIGN)) {
        parseError(p, "Expected '=' after for variable");
    }
    //parse the Start value and require the first comma
    ASTNode* start = parseExpression(p);
    if (!match(p, TOKEN_PUNC_COMMA)) {
        parseError(p, "Expected ',' after start value");
    }
    
    //parse the limit value
    ASTNode* end = parseExpression(p);
    ASTNode* step = NULL;
    
    //use a switch to check for an optional step parameter
    switch (peek(p).type) {
        case TOKEN_PUNC_COMMA:
            consume(p); //consume the second ','
            step = parseExpression(p);
            break;
        default:
            //if there is no comma, the defult is 1
            step = createNode(AST_NUMBER, (Token){TOKEN_NUMBER, "1", 0});
            break;
    }
    //consume do keyword
    if (!match(p, TOKEN_KW_DO)) {
        parseError(p, "Expected 'do' in for loop");
    }
    //parse the body of the loop
    ASTNode* body = parseBlock(p);
    if (!match(p, TOKEN_KW_END)) {
        parseError(p, "Expected 'end' to close for loop");
    }
    
    //construct the AST node
    ASTNode* node = createNode(AST_FOR, forToken);
    addChild(node, createNode(AST_IDENTIFIER, varName));
    addChild(node, start);
    addChild(node, end);
    addChild(node, step);
    addChild(node, body);
    return node;
}

ASTNode* parseFunctionDef(Parser* p) {
    //consume function keyword and function name
    Token funcToken = consume(p);
    Token name = consume(p);

    //error if no identifier or Lparen
    if (name.type != TOKEN_IDENTIFIER) parseError(p, "Expected function name");
    if (!match(p, TOKEN_PUNC_LPAREN)) parseError(p, "Expected '(' after function name");
    
    ASTNode* node = createNode(AST_FUNCTION_DECL, name);
    
    //add func params as children
    while (peek(p).type != TOKEN_PUNC_RPAREN && peek(p).type != TOKEN_EOF) {
        Token param = consume(p);
        if (param.type != TOKEN_IDENTIFIER) parseError(p, "Expected parameter name");
        addChild(node, createNode(AST_IDENTIFIER, param));
        //seperated by commas
        if (peek(p).type == TOKEN_PUNC_COMMA) consume(p);
    }
    //expecting Rparen
    if (peek(p).type == TOKEN_EOF) {
        reportError(PHASE_SYNTAX, peek(p).line, "Expected ')' to close function parameter list");
        return node;
    }
    consume(p); // consume the ')'
    
    addChild(node, parseBlock(p));
    if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' after function body");
    
    return node;
}
ASTNode* parseBlock(Parser* p) {
    //new level in the program, initialize its root
    ASTNode* blockNode = createNode(AST_BLOCK, (Token){TOKEN_EOF, "block", 0});
    
    //keep adding statemnts until the blocks end
    while (peek(p).type != TOKEN_KW_END && peek(p).type != TOKEN_KW_ELSE && peek(p).type != TOKEN_KW_ELSEIF && peek(p).type != TOKEN_KW_UNTIL && peek(p).type != TOKEN_EOF) {
        ASTNode* stmt = parseStatement(p);
        if (stmt) addChild(blockNode, stmt);
    }
    
    return blockNode;
}
// ==========================================
// Shunting Yard
// ==========================================
static int getPrecedence(TokenType type) {
    //precedence value, larger is stronger
    switch(type) {
        case TOKEN_KW_OR: return 1;
        case TOKEN_KW_AND: return 2;
        case TOKEN_OP_LT: case TOKEN_OP_GT: case TOKEN_OP_LTE: 
        case TOKEN_OP_GTE: case TOKEN_OP_NEQ: case TOKEN_OP_EQ: return 3;
        case TOKEN_OP_CONCAT: return 4;
        case TOKEN_OP_PLUS: case TOKEN_OP_MINUS: return 5;
        case TOKEN_OP_MUL: case TOKEN_OP_DIV: case TOKEN_OP_MOD: return 6;
        case TOKEN_OP_POW: return 7;
        default: return 0; //no math operator
    }
}
//helper for checking if a token can be in an legally expression
static bool isExpressionToken(TokenType type) {
    return (type == TOKEN_IDENTIFIER || type == TOKEN_NUMBER || type == TOKEN_STRING || type == TOKEN_PUNC_LPAREN || type == TOKEN_PUNC_RPAREN || type == TOKEN_KW_NIL || type == TOKEN_KW_TRUE || type == TOKEN_KW_FALSE
    || getPrecedence(type) > 0);
}
//checks if a token is a value/operand
static bool isOperandToken(TokenType type) {
    return (type == TOKEN_IDENTIFIER || type == TOKEN_NUMBER || type == TOKEN_STRING || type == TOKEN_PUNC_LPAREN ||type == TOKEN_KW_NIL || type == TOKEN_KW_TRUE || type == TOKEN_KW_FALSE);
}
//helper to check if there is a matching Lparen in the stack
static bool hasMatchingLeftParen(Token* opStack, int opTop) {
    for (int i = 0; i < opTop; i++) {
        if (opStack[i].type == TOKEN_PUNC_LPAREN) {
            return true;
        }
    }
    return false;
}

//pops an operator and its operands from the stacks, combines them into a
//single Abstract Syntax Tree (AST) node, and pushes the result back
static void popOperatorToTree(ASTNode** nodeStack, int* nodeTop, Token* opStack, int* opTop) {
    //We need at least 1 operator and 2 operands to make a binary tree
    if (*nodeTop < 2 || *opTop == 0) return;

    //LIFO, pop two operands and an operator
    Token op = opStack[--(*opTop)];
    ASTNode* right = nodeStack[--(*nodeTop)];
    ASTNode* left = nodeStack[--(*nodeTop)];
    //create a binary operator node
    ASTNode* binopNode = createNode(AST_BINOP, op);
    //add the children to the binary nodre
    addChild(binopNode, left);
    addChild(binopNode, right);
    //push the mini AST to the stack node
    nodeStack[(*nodeTop)++] = binopNode;
}
//parses an expression using the Shunting Yard algorithm to handle operator precedence.
//converts infix into an AST
ASTNode* parseExpression(Parser* p) {
    //initialize node and operator stacks
    ASTNode* nodeStack[128];
    int nodeTop = 0;
    Token opStack[128];
    int opTop = 0;
    //tracks what we expect after operand must come operator
    bool expectOperand = true;

    //continue parsing as long as the next token is valid inside an expression
    //and no two operands back to back
    //and if token id Rparen we need a Lparen in the stack
    while (isExpressionToken(peek(p).type) && 
           (expectOperand || !isOperandToken(peek(p).type)) &&
           (peek(p).type != TOKEN_PUNC_RPAREN || hasMatchingLeftParen(opStack, opTop))) {
        
        Token t = consume(p);

        switch (t.type) {
            //simple operands
            case TOKEN_NUMBER:
            case TOKEN_STRING:
            case TOKEN_KW_NIL:
            case TOKEN_KW_TRUE:
            case TOKEN_KW_FALSE: {
                //AST based on token type
                ASTNodeType type = AST_NUMBER; 
                if (t.type == TOKEN_STRING) type = AST_STRING;
                else if (t.type == TOKEN_KW_NIL) type = AST_NIL;
                else if (t.type == TOKEN_KW_TRUE || t.type == TOKEN_KW_FALSE) type = AST_IDENTIFIER;
                //add to nodestack
                nodeStack[nodeTop++] = createNode(type, t);
                expectOperand = false; //now we need an operator
                break;
            }
            
            //variables and function calls in expression
            case TOKEN_IDENTIFIER: {
                //handle function call
                if (peek(p).type == TOKEN_PUNC_LPAREN) {
                    //consume Lparen
                    consume(p); 
                    ASTNode* callNode = createNode(AST_FUNCTION_CALL, t);
                    //add func params as chidren
                    while (peek(p).type != TOKEN_PUNC_RPAREN && peek(p).type != TOKEN_EOF) {
                        addChild(callNode, parseExpression(p));
                        if (peek(p).type == TOKEN_PUNC_COMMA) consume(p); 
                    }
                    consume(p); // consume ')'
                    //add node to nodestack
                    nodeStack[nodeTop++] = callNode;
                } else {
                    //handle simple var
                    nodeStack[nodeTop++] = createNode(AST_IDENTIFIER, t);
                }
                expectOperand = false; //now we need operator
                break;
            }
            
            case TOKEN_PUNC_LPAREN: {
                opStack[opTop++] = t;
                expectOperand = true; 
                break;
            }
            
            //Rparen
            case TOKEN_PUNC_RPAREN: {
                //pop operators to the AST until we find the matching '('
                while (opTop > 0 && opStack[opTop - 1].type != TOKEN_PUNC_LPAREN) {
                    //as long as its not Lparen keep adding to the nodestack mini ASTs
                    popOperatorToTree(nodeStack, &nodeTop, opStack, &opTop);
                }
                if (opTop > 0) opTop--; //discard the '('
                expectOperand = false; //now operator
                break;
            }
            
            //operators
            default: {
                //as long as the previeus operator has larger precendence, add a mini AST to the nodestack
                if (getPrecedence(t.type) > 0) {
                    while (opTop > 0 && getPrecedence(opStack[opTop - 1].type) >= getPrecedence(t.type)) {
                        popOperatorToTree(nodeStack, &nodeTop, opStack, &opTop);
                    }
                    //add operator and now operand needed
                    opStack[opTop++] = t; 
                    expectOperand = true; 
                }
                break;
            }
        }
    }
    
    // cleanup
    while (opTop > 0) popOperatorToTree(nodeStack, &nodeTop, opStack, &opTop);
    
    if (nodeTop > 0) return nodeStack[nodeTop - 1];
    
    parseError(p, "Invalid or empty expression");
    return NULL;
}

// ==========================================
// printing the tree
// ==========================================
const char* getNodeTypeName(ASTNodeType type) {
    switch(type) {
        case AST_PROGRAM: return "AST_PROGRAM";
        case AST_BLOCK: return "AST_BLOCK";
        case AST_ASSIGNMENT: return "AST_ASSIGNMENT";
        case AST_LOCAL_ASSIGN: return "AST_LOCAL_ASSIGN";
        case AST_WHILE: return "AST_WHILE";
        case AST_FOR: return "AST_FOR";
        case AST_IF: return "AST_IF";
        case AST_REPEAT: return "AST_REPEAT";
        case AST_FUNCTION_DECL: return "AST_FUNCTION_DECL";
        case AST_FUNCTION_CALL: return "AST_FUNCTION_CALL";
        case AST_RETURN: return "AST_RETURN";
        case AST_BINOP: return "AST_BINOP";
        case AST_UNOP: return "AST_UNOP";
        case AST_IDENTIFIER: return "AST_IDENTIFIER";
        case AST_NUMBER: return "AST_NUMBER";
        case AST_STRING: return "AST_STRING";
        case AST_NIL: return "AST_NIL";
        default: return "UNKNOWN";
    }
}


void printAST(ASTNode* node, int depth) {
    if (!node) return;
    
    for (int i = 0; i < depth; i++) {
        printf("  |--");
    }
    printf("%s", getNodeTypeName(node->type));
    if (node->token.value) {
        printf(" ('%s')", node->token.value);
    }
    printf("\n");
    for (int i = 0; i < node->childCount; i++) {
        printAST(node->children[i], depth + 1);
    }
}