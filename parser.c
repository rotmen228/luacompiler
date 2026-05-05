#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "lexerH.h"
#include "ast.h"
#include "error_handler.h"

typedef struct {
    TokenList* list;
    int current;     // האינדקס של האסימון הנוכחי שאנחנו קוראים
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
// ==========================================
// חלק 1: ניהול זיכרון ומבנה העץ (AST)
// ==========================================

// פונקציה ליצירת צומת חדש בעץ
ASTNode* createNode(ASTNodeType type, Token token) {
    ASTNode* node = (ASTNode*)malloc(sizeof(ASTNode));
    if (!node) {
        printf("Memory allocation failed for ASTNode!\n");
        exit(1);
    }
    node->type = type;
    node->token = token;
    node->childCount = 0;
    node->childCapacity = 2; // מתחילים עם מקום ל-2 ילדים
    node->children = (ASTNode**)malloc(node->childCapacity * sizeof(ASTNode*));
    return node;
}

// פונקציה להוספת "ילד" לצומת "אבא" (למשל הוספת פקודה לתוך לולאת While)
void addChild(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;
    
    // אם המערך מלא, נכפיל את הגודל שלו (ניהול זיכרון דינמי כמו שלמדנו)
    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity *= 2;
        parent->children = (ASTNode**)realloc(parent->children, parent->childCapacity * sizeof(ASTNode*));
    }
    parent->children[parent->childCount++] = child;
}

// פונקציה רקורסיבית לשחרור כל העץ מהזיכרון בסיום הקימפול
void freeAST(ASTNode* root) {
    if (!root) return;
    for (int i = 0; i < root->childCount; i++) {
        freeAST(root->children[i]);
    }
    free(root->children);
    free(root);
}

// ==========================================
// חלק 2: מנגנון הניווט - Recursive Descent
// ==========================================



// פונקציית עזר: "הצצה" על האסימון הנוכחי בלי להתקדם
static Token peek(Parser* p) {
    if (p->current >= p->list->count) {
        return p->list->tokens[p->list->count - 1]; // מחזיר EOF
    }
    return p->list->tokens[p->current];
}

// פונקציית עזר: קריאת האסימון הנוכחי והתקדמות לאסימון הבא
static Token consume(Parser* p) {
    Token t = peek(p);
    p->current++;
    return t;
}

// פונקציית עזר: בדיקה האם האסימון הבא הוא מסוג מסוים. אם כן - בולע אותו.
static bool match(Parser* p, TokenType expectedType) {
    if (peek(p).type == expectedType) {
        consume(p);
        return true;
    }
    return false;
}

// פונקציה שזורקת שגיאת תחביר במקרה שהקוד ב-Lua לא תקין
static void parseError(Parser* p, const char* message) {
    Token t = peek(p);
    // אוספים את השגיאה האחרונה שגרמה לקריסה
    reportError(PHASE_SYNTAX, t.line, "%s (got '%s')", message, t.value ? t.value : "EOF");
    
    // מדפיסים את כל השגיאות שנאספו עד כה לפני שאנחנו יוצאים מהתוכנית!
    printAllErrors();
    exit(1);
}



// ==========================================
// הפונקציה הראשית של הפארסר (שורש העץ)
// ==========================================
ASTNode* runParser(TokenList* tokens) {
    Parser p = {tokens, 0};
    
    // יוצרים את צומת השורש של התוכנית
    Token dummyToken = {TOKEN_EOF, NULL, 0};
    ASTNode* programNode = createNode(AST_PROGRAM, dummyToken);
    
    // קוראים פקודות בלולאה כל עוד לא הגענו לסוף הקובץ
    while (peek(&p).type != TOKEN_EOF) {
        ASTNode* stmt = parseStatement(&p);
        if (stmt) {
            addChild(programNode, stmt);
        }
    }
    
    return programNode;
}
// ==========================================
// חלק 3: ניתוח פקודות (Statements)
// ==========================================

// הפונקציה המרכזית שמנתבת את סוג הפקודה
ASTNode* parseStatement(Parser* p) {
    Token t = peek(p);
    switch (t.type) {
        case TOKEN_KW_IF:       return parseIf(p);
        case TOKEN_KW_WHILE:    return parseWhile(p);
        case TOKEN_KW_FOR:      return parseFor(p);
        case TOKEN_KW_FUNCTION: return parseFunctionDef(p);
        case TOKEN_KW_RETURN:   return parseReturn(p);
        case TOKEN_KW_LOCAL:    return parseLocal(p);
        case TOKEN_KW_REPEAT:   return parseRepeat(p);
        case TOKEN_IDENTIFIER:  return parseAssignOrCall(p);
        default:
            printf("Syntax Error at line %d: Invalid statement (got '%s')\n", t.line, t.value ? t.value : "EOF");
            consume(p); // התקדמות כדי למנוע לולאה אינסופית בשגיאה
            return NULL;
    }
}
ASTNode* parseLocal(Parser* p) {
    consume(p); // בולע את המילה 'local'
    
    Token id = consume(p); // קריאת שם המשתנה (Identifier)
    if (id.type != TOKEN_IDENTIFIER) {
        parseError(p, "Expected variable name after 'local'");
    }

    // ---> התיקון הקריטי: יוצרים צומת מסוג זיהוי משתנה (Identifier)
    ASTNode* idNode = createNode(AST_IDENTIFIER, id);

    // בדיקה האם יש סימן שווה (אופציונלי בלואה - משתנה יכול להיות nil)
    if (match(p, TOKEN_OP_ASSIGN)) {
        ASTNode* valueNode = parseExpression(p);
        
        // יצירת צומת השמה מקומית
        ASTNode* assignNode = createNode(AST_LOCAL_ASSIGN, (Token){TOKEN_OP_ASSIGN, "=", p->list->tokens[p->current - 1].line});
        
        addChild(assignNode, idNode);
        addChild(assignNode, valueNode);
        return assignNode;
    } else {
        // אתחול ל-nil במידה ואין '='
        ASTNode* nilNode = createNode(AST_NIL, (Token){TOKEN_KW_NIL, "nil", p->list->tokens[p->current - 1].line});
        
        // יצירת צומת השמה מקומית
        ASTNode* assignNode = createNode(AST_LOCAL_ASSIGN, (Token){TOKEN_OP_ASSIGN, "=", p->list->tokens[p->current - 1].line});
        
        addChild(assignNode, idNode);
        addChild(assignNode, nilNode);
        return assignNode;
    }
}
// --- מימוש RETURN ---
ASTNode* parseReturn(Parser* p) {
    // 1. צרוך את האסימון return
    Token returnToken = consume(p); 
    
    ASTNode* valueNode = NULL;
    TokenType nextType = peek(p).type;
    
    // 2. בדוק אם האסימון הנוכחי הוא סוף בלוק (end, else, elseif, until, EOF)
    if (nextType == TOKEN_KW_END || 
        nextType == TOKEN_KW_ELSE || 
        nextType == TOKEN_KW_ELSEIF || 
        nextType == TOKEN_KW_UNTIL || 
        nextType == TOKEN_EOF) {
        
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
// --- מימוש השמה או קריאה לפונקציה ---
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
//fixed to support elseif
// --- מימוש IF (כולל תמיכה ב-ELSEIF) ---
ASTNode* parseIf(Parser* p) {
    Token ifToken = consume(p); // בולע 'if' או 'elseif'
    ASTNode* condition = parseExpression(p);
    
    if (!match(p, TOKEN_KW_THEN)) parseError(p, "Expected 'then' after condition");
    
    ASTNode* body = parseBlock(p);
    ASTNode* elseNode = NULL;
    
    // אם יש elseif, אנחנו קוראים שוב ל-parseIf! 
    // זה יבנה תת-עץ חדש של IF, ונחבר אותו בתור ה-ELSE שלנו.
    if (peek(p).type == TOKEN_KW_ELSEIF) {
        elseNode = parseIf(p); 
    } 
    else if (match(p, TOKEN_KW_ELSE)) {
        elseNode = parseBlock(p);
    }
    
    // רק ה-if הראשי והמקורי דורש end בסוף. 
    // ה-elseif הפנימי כבר "ירכב" על ה-end של ה-if הראשי.
    if (ifToken.type == TOKEN_KW_IF) {
        if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' to close if statement");
    }
    
    ASTNode* node = createNode(AST_IF, ifToken);
    addChild(node, condition);
    addChild(node, body);
    if (elseNode) addChild(node, elseNode);
    else addChild(node, createNode(AST_NIL, (Token){TOKEN_KW_NIL, "nil", 0}));
    
    return node;
}

// --- מימוש WHILE ---
ASTNode* parseWhile(Parser* p) {
    Token whileToken = consume(p);
    ASTNode* condition = parseExpression(p);
    
    if (!match(p, TOKEN_KW_DO)) parseError(p, "Expected 'do' after while condition");
    
    ASTNode* body = parseBlock(p);
    
    if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' to close while loop");
    
    ASTNode* node = createNode(AST_WHILE, whileToken);
    addChild(node, condition);
    addChild(node, body);
    return node;
}

// --- מימוש REPEAT ---
ASTNode* parseRepeat(Parser* p) {
    Token repeatToken = consume(p);
    ASTNode* body = parseBlock(p);
    
    if (!match(p, TOKEN_KW_UNTIL)) parseError(p, "Expected 'until' after repeat block");
    
    ASTNode* condition = parseExpression(p);
    
    ASTNode* node = createNode(AST_REPEAT, repeatToken);
    addChild(node, body);
    addChild(node, condition);
    return node;
}
// --- מימוש FOR ---
ASTNode* parseFor(Parser* p) {
    Token forToken = consume(p);
    Token varName = consume(p);
    if (varName.type != TOKEN_IDENTIFIER) parseError(p, "Expected variable name in for loop");
    
    if (!match(p, TOKEN_OP_ASSIGN)) parseError(p, "Expected '=' after for variable");
    
    ASTNode* start = parseExpression(p);
    if (!match(p, TOKEN_PUNC_COMMA)) parseError(p, "Expected ',' after start value");
    
    ASTNode* end = parseExpression(p);
    ASTNode* step = NULL;
    
    if (match(p, TOKEN_PUNC_COMMA)) {
        step = parseExpression(p);
    } else {
        // ברירת מחדל: צעד 1
        step = createNode(AST_NUMBER, (Token){TOKEN_NUMBER, "1", 0});
    }
    
    if (!match(p, TOKEN_KW_DO)) parseError(p, "Expected 'do' in for loop");
    ASTNode* body = parseBlock(p);
    if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' to close for loop");
    
    ASTNode* node = createNode(AST_FOR, forToken);
    addChild(node, createNode(AST_IDENTIFIER, varName));
    addChild(node, start);
    addChild(node, end);
    addChild(node, step);
    addChild(node, body);
    return node;
}

// --- מימוש הגדרת פונקציה ---
ASTNode* parseFunctionDef(Parser* p) {
    Token funcToken = consume(p);
    Token name = consume(p);
    if (name.type != TOKEN_IDENTIFIER) parseError(p, "Expected function name");
    
    if (!match(p, TOKEN_PUNC_LPAREN)) parseError(p, "Expected '(' after function name");
    
    ASTNode* node = createNode(AST_FUNCTION_DECL, name);
    
    // קריאת פרמטרים
    while (peek(p).type != TOKEN_PUNC_RPAREN) {
        Token param = consume(p);
        if (param.type != TOKEN_IDENTIFIER) parseError(p, "Expected parameter name");
        addChild(node, createNode(AST_IDENTIFIER, param));
        if (peek(p).type == TOKEN_PUNC_COMMA) consume(p);
    }
    consume(p); // צריכת ')'
    
    addChild(node, parseBlock(p));
    if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' after function body");
    
    return node;
}
ASTNode* parseBlock(Parser* p) {
    ASTNode* blockNode = createNode(AST_BLOCK, (Token){TOKEN_EOF, "block", 0});
    
    // ממשיכים לאסוף פקודות עד שנתקלים במילת סיום
    while (peek(p).type != TOKEN_KW_END && 
           peek(p).type != TOKEN_KW_ELSE && 
           peek(p).type != TOKEN_KW_ELSEIF && 
           peek(p).type != TOKEN_KW_UNTIL && 
           peek(p).type != TOKEN_EOF) {
        
        ASTNode* stmt = parseStatement(p);
        if (stmt) addChild(blockNode, stmt);
    }
    
    return blockNode;
}
// ==========================================
// חלק 4: ניתוח ביטויים (Shunting Yard)
// ==========================================

// פונקציית עזר: קבלת עדיפות האופרטור (Precedence) לפי חוקי Lua
static int getPrecedence(TokenType type) {
    switch(type) {
        case TOKEN_KW_OR: return 1;
        case TOKEN_KW_AND: return 2;
        case TOKEN_OP_LT: case TOKEN_OP_GT: case TOKEN_OP_LTE: 
        case TOKEN_OP_GTE: case TOKEN_OP_NEQ: case TOKEN_OP_EQ: return 3;
        case TOKEN_OP_CONCAT: return 4;
        case TOKEN_OP_PLUS: case TOKEN_OP_MINUS: return 5;
        case TOKEN_OP_MUL: case TOKEN_OP_DIV: case TOKEN_OP_MOD: return 6;
        case TOKEN_OP_POW: return 7;
        default: return 0; // לא אופרטור מתמטי
    }
}

// פונקציית עזר: האם האסימון הנוכחי שייך לביטוי מתמטי/לוגי?
static bool isExpressionToken(TokenType type) {
    return (type == TOKEN_IDENTIFIER || type == TOKEN_NUMBER || type == TOKEN_STRING ||
            type == TOKEN_PUNC_LPAREN || type == TOKEN_PUNC_RPAREN ||
            type == TOKEN_KW_NIL || type == TOKEN_KW_TRUE || type == TOKEN_KW_FALSE ||
            type == TOKEN_KW_FUNCTION ||
            getPrecedence(type) > 0);
}

// פונקציית עזר ל-Shunting Yard: שולפת אופרטור ו-2 ילדים, מחברת אותם ודוחפת חזרה
static void popOperatorToTree(ASTNode** nodeStack, int* nodeTop, Token* opStack, int* opTop) {
    if (*nodeTop < 2 || *opTop == 0) return; // הגנה משגיאות תחביר של המשתמש

    Token op = opStack[--(*opTop)];
    ASTNode* right = nodeStack[--(*nodeTop)];
    ASTNode* left = nodeStack[--(*nodeTop)];

    ASTNode* binopNode = createNode(AST_BINOP, op);
    addChild(binopNode, left);
    addChild(binopNode, right);

    // דוחפים את תת-העץ החדש חזרה למחסנית הצמתים
    nodeStack[(*nodeTop)++] = binopNode;
}

// הפונקציה המרכזית שמנתחת את הביטוי (Expression)
ASTNode* parseExpression(Parser* p) {
    ASTNode* nodeStack[128];
    int nodeTop = 0;
    Token opStack[128];
    int opTop = 0;
    
    bool expectOperand = true;

    while (isExpressionToken(peek(p).type)) {
        TokenType nextType = peek(p).type;
        
        // הגנה 1: עצירה בין שורות ללא נקודה-פסיק
        if (!expectOperand && (nextType == TOKEN_IDENTIFIER || nextType == TOKEN_NUMBER || 
                               nextType == TOKEN_STRING || nextType == TOKEN_PUNC_LPAREN ||
                               nextType == TOKEN_KW_NIL || nextType == TOKEN_KW_TRUE || 
                               nextType == TOKEN_KW_FALSE || nextType == TOKEN_KW_FUNCTION)) {
            break;
        }
        
        // הגנה 2: עצירה במקרה של סוגר ימני ששייך לפונקציה עוטפת
        if (nextType == TOKEN_PUNC_RPAREN) {
            bool hasMatchingParen = false;
            for (int i = 0; i < opTop; i++) {
                if (opStack[i].type == TOKEN_PUNC_LPAREN) {
                    hasMatchingParen = true;
                    break;
                }
            }
            if (!hasMatchingParen) break; 
        }

        Token t = consume(p);

        // מקרה 1: ערכים פשוטים (מספר, מחרוזת, בוליאני, nil)
        if (t.type == TOKEN_NUMBER || t.type == TOKEN_STRING || 
            t.type == TOKEN_KW_NIL || t.type == TOKEN_KW_TRUE || t.type == TOKEN_KW_FALSE) {
            
            ASTNodeType type = AST_NUMBER; 
            if (t.type == TOKEN_STRING) type = AST_STRING;
            else if (t.type == TOKEN_KW_NIL) type = AST_NIL;
            else if (t.type == TOKEN_KW_TRUE || t.type == TOKEN_KW_FALSE) type = AST_IDENTIFIER;
            
            nodeStack[nodeTop++] = createNode(type, t);
        } 
        // מקרה 1.5: פונקציה אנונימית (Closure) בתור ביטוי (למשל: return function(x))
        else if (t.type == TOKEN_KW_FUNCTION) {
            ASTNode* funcNode = createNode(AST_FUNCTION_DECL, t);
            if (!match(p, TOKEN_PUNC_LPAREN)) parseError(p, "Expected '(' for anonymous function");
            
            while (peek(p).type != TOKEN_PUNC_RPAREN && peek(p).type != TOKEN_EOF) {
                Token param = consume(p);
                if (param.type != TOKEN_IDENTIFIER) parseError(p, "Expected parameter name");
                addChild(funcNode, createNode(AST_IDENTIFIER, param));
                if (peek(p).type == TOKEN_PUNC_COMMA) consume(p);
            }
            consume(p); // בולע ')'
            
            addChild(funcNode, parseBlock(p));
            if (!match(p, TOKEN_KW_END)) parseError(p, "Expected 'end' to close anonymous function");
            
            nodeStack[nodeTop++] = funcNode;
        }
        // מקרה 2: מזהה (קריאה לפונקציה או משתנה)
        else if (t.type == TOKEN_IDENTIFIER) {
            if (peek(p).type == TOKEN_PUNC_LPAREN) {
                consume(p); 
                ASTNode* callNode = createNode(AST_FUNCTION_CALL, t);
                while (peek(p).type != TOKEN_PUNC_RPAREN && peek(p).type != TOKEN_EOF) {
                    addChild(callNode, parseExpression(p));
                    if (peek(p).type == TOKEN_PUNC_COMMA) consume(p); 
                }
                consume(p);
                nodeStack[nodeTop++] = callNode; 
            } else {
                nodeStack[nodeTop++] = createNode(AST_IDENTIFIER, t);
            }
        } 
        // מקרה 3: סוגר פותח
        else if (t.type == TOKEN_PUNC_LPAREN) {
            opStack[opTop++] = t;
        } 
        // מקרה 4: סוגר סוגר
        else if (t.type == TOKEN_PUNC_RPAREN) {
            while (opTop > 0 && opStack[opTop - 1].type != TOKEN_PUNC_LPAREN) {
                popOperatorToTree(nodeStack, &nodeTop, opStack, &opTop);
            }
            if (opTop > 0) opTop--; 
        } 
        // מקרה 5: אופרטור מתמטי/לוגי
        else if (getPrecedence(t.type) > 0) {
            while (opTop > 0 && getPrecedence(opStack[opTop - 1].type) >= getPrecedence(t.type)) {
                popOperatorToTree(nodeStack, &nodeTop, opStack, &opTop);
            }
            opStack[opTop++] = t; 
        }

        // --- עדכון סטטוס המטוטלת בסוף כל סיבוב ---
        if (t.type == TOKEN_IDENTIFIER || t.type == TOKEN_NUMBER || 
            t.type == TOKEN_STRING || t.type == TOKEN_PUNC_RPAREN ||
            t.type == TOKEN_KW_NIL || t.type == TOKEN_KW_TRUE || t.type == TOKEN_KW_FALSE ||
            t.type == TOKEN_KW_FUNCTION) {
            expectOperand = false; 
        } else if (getPrecedence(t.type) > 0 || t.type == TOKEN_PUNC_LPAREN) {
            expectOperand = true;  
        }
    }

    while (opTop > 0) popOperatorToTree(nodeStack, &nodeTop, opStack, &opTop);
    if (nodeTop > 0) return nodeStack[0];
    
    parseError(p, "Invalid or empty expression");
    return NULL;
}
// ==========================================
// חלק 5: הדפסת העץ (לצורכי דיבאגינג)
// ==========================================

// פונקציית עזר להמרת סוג הצומת למחרוזת קריאה
const char* getNodeTypeName(ASTNodeType type) {
    switch(type) {
        case AST_PROGRAM: return "AST_PROGRAM";
        case AST_BLOCK: return "AST_BLOCK";
        case AST_ASSIGNMENT: return "AST_ASSIGNMENT";
        case AST_LOCAL_ASSIGN: return "AST_LOCAL_ASSIGN"; // <--- הוסף את השורה הזו
        case AST_IF: return "AST_IF";
        case AST_WHILE: return "AST_WHILE";
        case AST_FOR: return "AST_FOR";
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

// פונקציה רקורסיבית להדפסת העץ עם הזחות (אינדנטציה) לפי העומק
void printAST(ASTNode* node, int depth) {
    if (!node) return;
    
    // הדפסת רווחים לפי עומק הצומת בעץ
    for (int i = 0; i < depth; i++) {
        printf("  |--");
    }
    
    // הדפסת סוג הצומת
    printf("%s", getNodeTypeName(node->type));
    
    // אם יש לטוקן ערך (כמו שם משתנה, מספר או אופרטור), נדפיס גם אותו
    if (node->token.value) {
        printf(" ('%s')", node->token.value);
    }
    printf("\n");
    
    // קריאה רקורסיבית לכל הילדים
    for (int i = 0; i < node->childCount; i++) {
        printAST(node->children[i], depth + 1);
    }
}