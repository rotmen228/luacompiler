#ifndef AST_H
#define AST_H

#include "lexerH.h" // אנחנו צריכים את מבנה האסימון (Token) שהגדרנו קודם

// סוגי הצמתים בעץ (מבוסס על עמוד 96 בספר שלך, בתוספת תיקונים הנדסיים)
typedef enum {
    AST_PROGRAM,        // שורש העץ (כל התוכנית)
    AST_BLOCK,          // בלוק של קוד (למשל בתוך if או while)
    AST_ASSIGNMENT,     // השמת משתנה (x = 5)
    AST_IF,             // תנאי
    AST_WHILE,
    AST_REPEAT,
    AST_FOR,
    AST_FUNCTION_DECL,  // הגדרת פונקציה
    AST_FUNCTION_CALL,  // קריאה לפונקציה
    AST_RETURN,         // החזרת ערך
    
    AST_BINOP,          // פעולה בינארית (+, -, *, /, ==, <)
    AST_UNOP,           // פעולה אונארית (למשל השלילה not)
    
    AST_IDENTIFIER,     // שם משתנה
    AST_NUMBER,         // מספר
    AST_STRING,         // מחרוזת
    AST_NIL             // ערך ריק (תוקן מ-AST NIL)
} ASTNodeType;

// מבנה צומת בעץ
typedef struct ASTNode {
    ASTNodeType type;
    Token token;                 // האסימון המקורי שיצר את הצומת (לשמירת הערך ומספר השורה)
    
    // ניהול היררכיה: מערך דינמי של ילדים (כדי לתמוך בפונקציות עם המון פרמטרים או בלוקים ארוכים)
    struct ASTNode** children;   
    int childCount;
    int childCapacity;
} ASTNode;

// חתימות לפונקציות ניהול הזיכרון של העץ
ASTNode* createNode(ASTNodeType type, Token token);
void addChild(ASTNode* parent, ASTNode* child);
void freeAST(ASTNode* root);
void printAST(ASTNode* node, int depth);
ASTNode* runParser(TokenList* tokens);

#endif // AST_H