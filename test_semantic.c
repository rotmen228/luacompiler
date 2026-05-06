#include <stdio.h>
#include <stdlib.h>
#include "lexerH.h"
#include "ast.h"
#include "semantic.h"
#include "error_handler.h"

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

int main(int argc, char* argv[]) {
    // דורש לקבל את שם הקובץ משורת הפקודה
    if (argc < 2) {
        printf("Usage: test_semantic <input_file.lua>\n");
        return 1;
    }

    const char* filename = argv[1];

    initErrorHandler();

    // 1. קרא את הקובץ
    char* source_code = readFile(filename);
    if (!source_code) {
        return 1;
    }

    printf("Starting Full Semantic Analysis for: %s\n", filename);

    // 2. הרץ את הלקסר כדי לייצר את האסימונים
    TokenList token_list = runLexer(source_code);

    // 3. הרץ את הפארסר על האסימונים ליצירת עץ תחביר (AST)
    ASTNode* syntax_tree = runParser(&token_list);

    // 4. אם העץ נוצר בהצלחה, הרץ את המנתח הסמנטי
    SymbolTable* global_symbol_table = NULL;
    if (syntax_tree) {
        global_symbol_table = analyzeSemantic(syntax_tree);
        
        // 5. הדפס את כל טבלאות הסמלים (הסקופים) שנוצרו
        if (global_symbol_table) {
            printFinalSymbolTables(global_symbol_table);
        }
    }

    // 6. אם היו שגיאות (לקסיקליות, תחביריות או סמנטיות) - הדפס את כולן
    if (hasErrors()) {
        printAllErrors();
    }

    // 7. נקה זיכרון
    if (syntax_tree) {
        freeAST(syntax_tree);
    }
    freeTokenList(&token_list);
    free(source_code);
    freeErrorHandler();

    return 0;
}