#include <stdio.h>
#include <stdlib.h>
#include "lexerH.h"
#include "ast.h"
#include "error_handler.h"

// פונקציית עזר לקריאת התוכן של הקובץ (בדיוק כמו בלקסר וב-main)
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

int main(int argc, char* argv[]) {
    // דורש לקבל את שם הקובץ משורת הפקודה
    if (argc < 2) {
        printf("Usage: test_parser <input_file.lua>\n");
        return 1;
    }

    const char* filename = argv[1];

    initErrorHandler();

    // 1. קרא את הקובץ
    char* source_code = readFile(filename);
    if (!source_code) {
        return 1;
    }

    printf("Starting Parser analysis for: %s\n", filename);

    // 2. הרץ את הלקסר כדי לייצר את האסימונים
    TokenList token_list = runLexer(source_code);

    // 3. הרץ את הפארסר על האסימונים
    ASTNode* syntax_tree = runParser(&token_list);

    // 4. הדפס את העץ (אם נוצר בהצלחה)
    if (syntax_tree) {
        printf("\n===========================================================\n");
        printf("   ABSTRACT SYNTAX TREE (AST)\n");
        printf("===========================================================\n");
        printAST(syntax_tree, 0);
        printf("===========================================================\n\n");
    }

    // 5. אם היו שגיאות (לקסיקליות או תחביריות) - הדפס אותן
    if (hasErrors()) {
        printAllErrors();
    }

    // 6. נקה זיכרון
    if (syntax_tree) {
        freeAST(syntax_tree);
    }
    freeTokenList(&token_list);
    free(source_code);
    freeErrorHandler();

    return 0;
}