#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexerH.h"
#include "ast.h"
#include "semantic.h"
#include "codegen.h"
#include "error_handler.h" // <--- הוספנו את מנהל השגיאות!

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

static void makeOutputName(const char* inputName, char* outBuf, int outBufSize) {
    const char* dot = strrchr(inputName, '.');
    int baseLen = dot ? (int)(dot - inputName) : (int)strlen(inputName);
    if (baseLen >= outBufSize - 10) baseLen = outBufSize - 10;
    strncpy(outBuf, inputName, baseLen);
    outBuf[baseLen] = '\0';
    strncat(outBuf, "_output.c", outBufSize - baseLen - 1);
}

static const char* identifyScopeName(SymbolTable* target, SymbolTable* globalScope) {
    if (!globalScope) return NULL;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = globalScope->buckets[i];
        while (entry) {
            if (entry->record->type == TYPE_FUNCTION) {
                SymbolTable* funcTable = getFuncScope(entry->record->name);
                if (funcTable == target) return entry->record->name;
            }
            entry = entry->next;
        }
    }
    return NULL;
}

static void printAllScopesRecursive(SymbolTable* table, const char* scopeName, SymbolTable* globalScope) {
    if (!table) return;
    printSymbolTable(table, scopeName);
    for (int i = 0; i < table->childCount; i++) {
        char childName[256];
        SymbolTable* childTable = table->children[i];
        const char* funcName = identifyScopeName(childTable, globalScope);
        
        if (funcName) snprintf(childName, sizeof(childName), "Function: %s", funcName);
        else snprintf(childName, sizeof(childName), "%s -> Sub-Block %d", scopeName, i + 1);
        
        printAllScopesRecursive(childTable, childName, globalScope);
    }
}

int main(void) {
    const char* files[] = {"test_simple.lua", "test_mid.lua", "test_hard.lua", "test_midS.lua"};
    int numFiles = (int)(sizeof(files) / sizeof(files[0]));

    for (int i = 0; i < numFiles; i++) {
        printf("\n*************************************************************\n");
        printf("  PROCESSING: %s\n", files[i]);
        printf("*************************************************************\n");

        char* source = readFile(files[i]);
        if (!source) continue;

        // אתחול רשימת השגיאות עבור הקובץ הנוכחי
        initErrorHandler();

        printf("\n[1/4] Running Lexer...\n");
        TokenList list = runLexer(source);

        printf("\n[2/4] Building AST...\n");
        ASTNode* root = runParser(&list);

        if (root) {
            printf("\n[3/4] Running Semantic Analysis...\n");
            SymbolTable* globalScope = analyzeSemantic(root);

            // בדיקה האם נאספו שגיאות במהלך הלקסר, הפארסר או הניתוח הסמנטי
            if (hasErrors()) {
                printAllErrors();
            } else {
                if (globalScope) {
                    printf("\n=== FULL SCOPE TREE ===\n");
                    printAllScopesRecursive(globalScope, "Global Scope", globalScope);
                }

                char outputName[256];
                makeOutputName(files[i], outputName, sizeof(outputName));
                printf("\n[4/4] Generating C code → %s\n", outputName);
                generateCode(root, globalScope, outputName);
            }
        } else {
            // אם העץ לא נוצר (שגיאת Syntax קריטית) נדפיס את השגיאות
            printAllErrors();
        }

        // ניקוי זיכרון
        freeAST(root);
        freeTokenList(&list);
        free(source);
        freeErrorHandler();
        
        printf("\n  Done with %s\n", files[i]);
    }

    printf("\n=============================================================\n");
    printf("  All files processed.\n");
    printf("=============================================================\n");
    return 0;
}