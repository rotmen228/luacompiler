#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexerH.h"
#include "ast.h"
#include "semantic.h"
#include "codegen.h"

// ============================================================
// Token name table (for debug printing)
// ============================================================
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
    "TOKEN_PUNC_LBRACE",  "TOKEN_PUNC_RBRACE",
    "TOKEN_PUNC_LBRACKET","TOKEN_PUNC_RBRACKET",
    "TOKEN_PUNC_COMMA", "TOKEN_PUNC_SEMI", "TOKEN_PUNC_COLON",

    "TOKEN_IDENTIFIER", "TOKEN_NUMBER", "TOKEN_STRING",
    "TOKEN_EOF", "TOKEN_ERROR"
};

// ============================================================
// Helper: read a whole file into a malloc'd string
// ============================================================
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
    if (!buffer) {
        printf("Error: Memory allocation failed for '%s'\n", filename);
        fclose(file);
        return NULL;
    }
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);
    return buffer;
}

// ============================================================
// Helper: build an output filename like "test_simple_output.c"
// ============================================================
static void makeOutputName(const char* inputName, char* outBuf, int outBufSize) {
    const char* dot = strrchr(inputName, '.');
    int baseLen = dot ? (int)(dot - inputName) : (int)strlen(inputName);
    if (baseLen >= outBufSize - 10) baseLen = outBufSize - 10;
    strncpy(outBuf, inputName, baseLen);
    outBuf[baseLen] = '\0';
    strncat(outBuf, "_output.c", outBufSize - baseLen - 1);
}

// ============================================================
// NEW: Scope Tree Traversal (Recursive Printing)
// ============================================================

// פונקציית עזר לבדיקה אם בלוק מסוים הוא בעצם פונקציה מוכרת
static const char* identifyScopeName(SymbolTable* target, SymbolTable* globalScope) {
    if (!globalScope) return NULL;
    
    // סורקים את הטבלה הגלובלית כדי למצוא פונקציות
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = globalScope->buckets[i];
        while (entry) {
            if (entry->record->type == TYPE_FUNCTION) {
                // שולפים את הטבלה של הפונקציה ובודקים אם היא הטבלה שאנחנו מסתכלים עליה עכשיו
                SymbolTable* funcTable = getFuncScope(entry->record->name);
                if (funcTable == target) {
                    return entry->record->name; // מצאנו!
                }
            }
            entry = entry->next;
        }
    }
    return NULL;
}

// הפונקציה הרקורסיבית שעוברת על כל עץ הטבלאות
static void printAllScopesRecursive(SymbolTable* table, const char* scopeName, SymbolTable* globalScope) {
    if (!table) return;
    
    // 1. מדפיסים את הטבלה הנוכחית
    printSymbolTable(table, scopeName);
    
    // 2. עוברים על כל הילדים (תנאי IF, לולאות, או פונקציות פנימיות) ומדפיסים גם אותם
    for (int i = 0; i < table->childCount; i++) {
        char childName[256];
        SymbolTable* childTable = table->children[i];
        
        // ננסה להבין אם הילד הזה הוא בעצם פונקציה
        const char* funcName = identifyScopeName(childTable, globalScope);
        
        if (funcName) {
            snprintf(childName, sizeof(childName), "Function: %s", funcName);
        } else {
            // אם זה לא פונקציה, זה כנראה בלוק אנונימי (כמו if או while)
            snprintf(childName, sizeof(childName), "%s -> Sub-Block %d", scopeName, i + 1);
        }
        
        printAllScopesRecursive(childTable, childName, globalScope);
    }
}

// ============================================================
// main
// ============================================================
int main(void) {
    const char* files[] = {"test_simple.lua", "test_mid.lua", "test_hard.lua", "test_hardS.lua"};
    int numFiles = (int)(sizeof(files) / sizeof(files[0]));

    for (int i = 0; i < numFiles; i++) {

        printf("\n");
        printf("*************************************************************\n");
        printf("  PROCESSING: %s\n", files[i]);
        printf("*************************************************************\n");

        // ----------------------------------------------------------
        // Step 1: Read source file
        // ----------------------------------------------------------
        char* source = readFile(files[i]);
        if (!source) continue;

        // ----------------------------------------------------------
        // Step 2: Lexical Analysis
        // ----------------------------------------------------------
        printf("\n[1/4] Running Lexer...\n");
        TokenList list = runLexer(source);
        printf("      Produced %d tokens.\n", list.count);

        // ----------------------------------------------------------
        // Step 3: Parsing → AST
        // ----------------------------------------------------------
        printf("\n[2/4] Building AST...\n");
        ASTNode* root = runParser(&list);

        if (!root) {
            printf("      ERROR: Failed to build AST for '%s'. Skipping.\n", files[i]);
            freeTokenList(&list);
            free(source);
            continue;
        }

        // Print the full AST
        printf("\n--- Abstract Syntax Tree ---\n");
        printAST(root, 0);

        // ----------------------------------------------------------
        // Step 4: Semantic Analysis → Symbol Table
        // ----------------------------------------------------------
        printf("\n[3/4] Running Semantic Analysis...\n");
        SymbolTable* globalScope = analyzeSemantic(root);

        // הדפסת כל טבלאות הסמלים באמצעות העץ הרקורסיבי שלנו!
        if (globalScope) {
            printf("\n=== FULL SCOPE TREE (INCLUDING IFs & LOOPs) ===\n");
            printAllScopesRecursive(globalScope, "Global Scope", globalScope);
        } else {
            printf("      WARNING: Semantic analysis returned NULL table.\n");
        }

        // ----------------------------------------------------------
        // Step 5: Code Generation → output file
        // ----------------------------------------------------------
        char outputName[256];
        makeOutputName(files[i], outputName, sizeof(outputName));

        printf("\n[4/4] Generating C code → %s\n", outputName);
        generateCode(root, globalScope, outputName);

        // ----------------------------------------------------------
        // Cleanup
        // ----------------------------------------------------------
        freeAST(root);
        freeTokenList(&list);
        free(source);

        printf("\n  Done with %s\n", files[i]);
        printf("-------------------------------------------------------------\n");
    }

    printf("\n=============================================================\n");
    printf("  All files processed.\n");
    printf("=============================================================\n");
    return 0;
}