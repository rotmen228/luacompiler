#include <stdio.h>
#include <stdlib.h>
#include "lexerH.h"
#include "ast.h"      // הספריה של העץ
#include "semantic.h" // הוספנו את הספריה הסמנטית!

const char* tokenNames[] = {
    "TOKEN_KW_IF", "TOKEN_KW_THEN", "TOKEN_KW_ELSEIF", "TOKEN_KW_ELSE", "TOKEN_KW_END",
    "TOKEN_KW_WHILE", "TOKEN_KW_DO", "TOKEN_KW_FOR", "TOKEN_KW_REPEAT", "TOKEN_KW_UNTIL",
    "TOKEN_KW_FUNCTION", "TOKEN_KW_RETURN", "TOKEN_KW_LOCAL",
    "TOKEN_KW_TRUE", "TOKEN_KW_FALSE", "TOKEN_KW_NIL",
    "TOKEN_KW_AND", "TOKEN_KW_OR", "TOKEN_KW_NOT",
    
    "TOKEN_OP_PLUS", "TOKEN_OP_MINUS", "TOKEN_OP_MUL", "TOKEN_OP_DIV", "TOKEN_OP_MOD", "TOKEN_OP_POW",
    "TOKEN_OP_EQ", "TOKEN_OP_NEQ", "TOKEN_OP_LT", "TOKEN_OP_GT", "TOKEN_OP_LTE", "TOKEN_OP_GTE",
    "TOKEN_OP_ASSIGN", "TOKEN_OP_CONCAT", "TOKEN_OP_LEN",
    
    "TOKEN_PUNC_LPAREN", "TOKEN_PUNC_RPAREN",
    "TOKEN_PUNC_LBRACE", "TOKEN_PUNC_RBRACE",
    "TOKEN_PUNC_LBRACKET", "TOKEN_PUNC_RBRACKET",
    "TOKEN_PUNC_COMMA", "TOKEN_PUNC_SEMI", "TOKEN_PUNC_COLON",
    
    "TOKEN_IDENTIFIER", "TOKEN_NUMBER", "TOKEN_STRING",
    "TOKEN_EOF", "TOKEN_ERROR"
};

// פונקציית עזר לקריאת תוכן של קובץ לתוך מחרוזת (Buffer)
char* readFile(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file %s\n", filename);
        return NULL;
    }

    // מציאת גודל הקובץ
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    // הקצאת זיכרון
    char* buffer = (char*)malloc(length + 1);
    if (!buffer) {
        printf("Error: Memory allocation failed for file %s\n", filename);
        fclose(file);
        return NULL;
    }

    // קריאת התוכן
    fread(buffer, 1, length, file);
    buffer[length] = '\0';

    fclose(file);
    return buffer;
}

// פונקציה להדפסת האסימונים בצורה טבלאית
void printTokens(const char* testName, const TokenList* list) {
    printf("\n=================================================\n");
    printf("   RESULTS FOR: %s\n", testName);
    printf("=================================================\n");
    printf("%-10s | %-20s | %s\n", "Line", "Token Name", "Value");
    printf("-------------------------------------------------\n");
    
    for (int i = 0; i < list->count; i++) {
        printf("%-10d | %-20s | '%s'\n", 
               list->tokens[i].line, 
               tokenNames[list->tokens[i].type], 
               list->tokens[i].value ? list->tokens[i].value : "NULL");
    }
    printf("=================================================\n\n");
}

int main() {
    // מערך של שמות הקבצים לבדיקה
    const char* files[] = {"test_simple.lua", "test_mid.lua", "test_hard.lua"};
    int numFiles = sizeof(files) / sizeof(files[0]);

    for (int i = 0; i < numFiles; i++) {
        printf("\n*************************************************\n");
        printf("Starting analysis for: %s...\n", files[i]);
        printf("*************************************************\n");
        
        char* source = readFile(files[i]);
        if (source) {
            // 1. הרצת הלקסר
            TokenList list = runLexer(source);
            
            // הדפסת תוצאות הלקסר (אופציונלי, אפשר למחוק/להעיר אם זה מדפיס יותר מדי)
            // printTokens(files[i], &list);
            
            // 2. הרצת הפארסר
            printf("Building Abstract Syntax Tree (AST) for %s...\n", files[i]);
            printf("-------------------------------------------------\n");
            ASTNode* root = runParser(&list);
            
            // 3. הדפסת העץ
            // 3. הדפסת העץ
            if (root) {
                printf("\n--- Abstract Syntax Tree (AST) ---\n");
                printAST(root, 0); // <-- הסרנו את ההערה, עכשיו העץ יודפס!
                
                // 4. הרצת הניתוח הסמנטי (Semantic Analysis)
                SymbolTable* globalScope = analyzeSemantic(root);
                
                // 5. הדפסת טבלת הסמלים הגלובלית בסיום
                if (globalScope != NULL) {
                    printSymbolTable(globalScope, "Global Scope");
                }
                
            } else {
                printf("Failed to build AST.\n");
            }
            
            // שחרור זיכרון - קודם העץ ואז האסימונים
            freeAST(root);
            freeTokenList(&list);
            free(source);
        }
    }

    printf("\nAll tests completed.\n");
    return 0;
}