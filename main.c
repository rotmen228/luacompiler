#include <stdio.h>
#include <stdlib.h>
#include "lexerH.h"

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
    printf("%-10s | %-20s | %s\n", "Line", "Token Type", "Value");
    printf("-------------------------------------------------\n");
    
    for (int i = 0; i < list->count; i++) {
        printf("%-10d | %-20d | '%s'\n", 
               list->tokens[i].line, 
               list->tokens[i].type, 
               list->tokens[i].value ? list->tokens[i].value : "NULL");
    }
    printf("=================================================\n\n");
}

int main() {
    // מערך של שמות הקבצים לבדיקה
    const char* files[] = {"test_simple.lua", "test_mid.lua", "test_hard.lua"};
    int numFiles = sizeof(files) / sizeof(files[0]);

    for (int i = 0; i < numFiles; i++) {
        printf("Starting analysis for: %s...\n", files[i]);
        
        char* source = readFile(files[i]);
        if (source) {
            // הרצת הלקסר (הפרוצדורה המרכזית מהספר שלך)
            TokenList list = runLexer(source);
            
            // הדפסת תוצאות
            printTokens(files[i], &list);
            
            // שחרור זיכרון
            freeTokenList(&list);
            free(source);
        }
    }

    printf("All tests completed.\n");
    return 0;
}