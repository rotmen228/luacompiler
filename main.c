#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexerH.h"
#include "ast.h"
#include "semantic.h"
#include "codegen.h"
#include "error_handler.h"

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
    // 1. חלץ את נתיב_קובץ_מקור ואת נתיב_קובץ_פלט מה-CLI
    if (argc < 2) {
        printf("Usage: luacompiler <input_file.lua> [-o <output_file.c>]\n");
        return 1;
    }

    const char* source_file_path = argv[1];
    const char* output_file_path = "output.c"; // ברירת מחדל

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file_path = argv[i + 1];
            break;
        }
    }

    initErrorHandler();

    // 2. פתח את נתיב_קובץ_מקור, קרא את כל תוכנו וסגור אותו
    char* source_code = readFile(source_file_path);
    if (!source_code) {
        return 1;
    }

    // 3. קרא לפונקציה runLexer עם קוד_מקור
    TokenList token_list = runLexer(source_code);

    // 4. אם קיימת שגיאה במודל - הדפס
    if (hasErrors()) {
        printAllErrors();
    }

    // 5. קרא לפונקציה runProgram (runParser אצלנו) עם רשימת_אסימונים
    ASTNode* syntax_tree = runParser(&token_list);

    // 6. אם קיימת שגיאה במודל - הדפס
    if (hasErrors()) {
        printAllErrors();
    }

    // 7. קרא לפונקציה analyzeSemantic על עץ_תחביר
    SymbolTable* global_symbol_table = NULL;
    if (syntax_tree) {
        global_symbol_table = analyzeSemantic(syntax_tree);
    }

    // 8. אם קיימת שגיאה במודל - הדפס
    if (hasErrors()) {
        printAllErrors();
    }

    // 9. קרא לפונקציה generateCode (המחרוזת נשמרת בבאפר_קוד_C)
    char* c_code_buffer = NULL;
    if (syntax_tree && global_symbol_table) {
        c_code_buffer = generateCode(syntax_tree, global_symbol_table);
    }

    if (c_code_buffer) {
        // 10. פתח קובץ חדש בדיסק בשם נתיב_קובץ_פלט
        FILE* out_file = fopen(output_file_path, "w");
        if (out_file) {
            // 11. כתוב את התוכן של חוצץ_קוד_C לתוך הקובץ וסגור אותו
            fputs(c_code_buffer, out_file);
            fclose(out_file);
            
            // 12. הדפס הודעת הצלחה ב-View
            printf("Compilation successful! Output saved to: %s\n", output_file_path);
        } else {
            printf("Error: Could not write to output file '%s'\n", output_file_path);
        }
        
        // שחרור המחרוזת שנוצרה ב-codegen
        free(c_code_buffer);
    }

    // 13. שחרר את כל הזיכרון שהוקצה דינמית
    if (syntax_tree) freeAST(syntax_tree);
    freeTokenList(&token_list);
    if (source_code) free(source_code);
    freeErrorHandler();
    

    if (global_symbol_table) freeSymbolTable(global_symbol_table);
    // 14. החזר 0
    return 0;
}