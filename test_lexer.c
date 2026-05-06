#include <stdio.h>
#include <stdlib.h>
#include "lexerH.h"
#include "error_handler.h"

// מערך שממפה את מספרי ה-enum של הטיפוסים למחרוזות קריאות לבני אדם
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

// פונקציית עזר לקריאת התוכן של הקובץ
char* readFile(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Could not open file '%s'\n", filename);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

// פונקציה להדפסת רשימת האסימונים בטבלה
void printTokens(const char* testName, const TokenList* list) {
    printf("\n===========================================================\n");
    printf("   LEXER RESULTS FOR: %s\n", testName);
    printf("===========================================================\n");
    printf("%-10s | %-25s | %s\n", "Line", "Token Type", "Value");
    printf("-----------------------------------------------------------\n");
    
    for (int i = 0; i < list->count; i++) {
        printf("%-10d | %-25s | '%s'\n", 
               list->tokens[i].line, 
               tokenNames[list->tokens[i].type], 
               list->tokens[i].value ? list->tokens[i].value : "NULL");
    }
    printf("===========================================================\n\n");
}

int main(int argc, char* argv[]) {
    // דורש לקבל את שם הקובץ משורת הפקודה
    if (argc < 2) {
        printf("Usage: test_lexer <input_file.lua>\n");
        return 1;
    }

    const char* filename = argv[1];

    initErrorHandler();

    // 1. קרא את הקובץ
    char* source_code = readFile(filename);
    if (!source_code) {
        return 1;
    }

    printf("Starting Lexer analysis...\n");

    // 2. הרץ את הלקסר
    TokenList token_list = runLexer(source_code);

    // 3. הדפס את הטבלה
    printTokens(filename, &token_list);

    // 4. אם היו שגיאות (כמו תווים לא חוקיים או מחרוזות שלא נסגרו) - הדפס אותן
    if (hasErrors()) {
        printAllErrors();
    }

    // 5. נקה זיכרון
    freeTokenList(&token_list);
    free(source_code);
    freeErrorHandler();

    return 0;
}