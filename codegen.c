#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"

// ============================================================
// Output buffer — grows dynamically, holds the whole C file
// ============================================================
typedef struct {
    char* data;
    int   length;
    int   capacity;
} OutputBuffer;

static OutputBuffer g_buf;

static void buf_init(void) {
    g_buf.capacity = 4096;
    g_buf.length   = 0;
    g_buf.data     = (char*)malloc(g_buf.capacity);
    g_buf.data[0]  = '\0';
}

static void buf_append(const char* str) {
    int addLen = (int)strlen(str);
    while (g_buf.length + addLen + 1 > g_buf.capacity) {
        g_buf.capacity *= 2;
        g_buf.data = (char*)realloc(g_buf.data, g_buf.capacity);
    }
    memcpy(g_buf.data + g_buf.length, str, addLen + 1);
    g_buf.length += addLen;
}

// ============================================================
// Forward declarations
// ============================================================
static void generateBlock      (ASTNode* node,  int indent, SymbolTable* table);
static void generateAssign     (ASTNode* node,  int indent, SymbolTable* table);
static void generateLocalAssign(ASTNode* node,  int indent, SymbolTable* table);
static void generateIf         (ASTNode* node,  int indent, SymbolTable* table);
static void generateLoop       (ASTNode* node,  int indent, SymbolTable* table);
static void generateFor        (ASTNode* node,  int indent, SymbolTable* table);
static void generateFunction   (ASTNode* node,  int indent, SymbolTable* table);
static void generateCall       (ASTNode* node,  int indent, SymbolTable* table);
static void generateReturn     (ASTNode* node,  int indent, SymbolTable* table);
static void generateExpression (ASTNode* node,  SymbolTable* table);
static void generateGlobalDeclarations(SymbolTable* globalTable);
static void generateFunctions  (ASTNode* root,  SymbolTable* globalTable);

// ============================================================
// Helpers
// ============================================================

static void write_indent(int indent) {
    char spaces[128] = {0};
    int  n = indent * 4;
    if (n > 127) n = 127;
    for (int i = 0; i < n; i++) spaces[i] = ' ';
    buf_append(spaces);
}

static const char* cTypeName(SymbolType type) {
    switch (type) {
        case TYPE_INT:    return "int";
        case TYPE_DOUBLE: return "double";
        case TYPE_STRING: return "char*";
        case TYPE_BOOL:   return "int";    // Lua booleans → C int (0/1)
        case TYPE_VOID:   return "void";
        default:          return "void*";  // TYPE_UNKNOWN → safe fallback
    }
}

static const char* printfFmt(SymbolType type) {
    switch (type) {
        case TYPE_INT:    return "%d";
        case TYPE_DOUBLE: return "%f";
        case TYPE_STRING: return "%s";
        case TYPE_BOOL:   return "%d";
        default:          return "%p";
    }
}

// ============================================================
// generateExpression
// ============================================================
static void generateExpression(ASTNode* node, SymbolTable* table) {
    if (!node) { buf_append("NULL"); return; }

    switch (node->type) {

        case AST_NIL:
            buf_append("NULL");
            break;

        // FIX #2: numbers are emitted raw; strings are wrapped in quotes.
        case AST_NUMBER:
            buf_append(node->token.value);
            break;

        case AST_STRING:
            buf_append("\"");
            buf_append(node->token.value);
            buf_append("\"");
            break;

        case AST_IDENTIFIER:
            if (strcmp(node->token.value, "true")  == 0) { buf_append("1"); break; }
            if (strcmp(node->token.value, "false") == 0) { buf_append("0"); break; }
            buf_append(node->token.value);
            break;

        case AST_BINOP: {
            ASTNode* left  = node->children[0];
            ASTNode* right = node->children[1];

            // Lua concat (..) → concat_strings(left, right)
            if (node->token.type == TOKEN_OP_CONCAT) {
                buf_append("concat_strings(");
                generateExpression(left,  table);
                buf_append(", ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // Lua ~= → !=
            if (node->token.type == TOKEN_OP_NEQ) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" != ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // FIX #1: Lua `and` → C `&&`
            if (node->token.type == TOKEN_KW_AND) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" && ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // FIX #1: Lua `or` → C `||`
            if (node->token.type == TOKEN_KW_OR) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" || ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // All other operators: emit (left op right)
            buf_append("(");
            generateExpression(left, table);
            buf_append(" ");
            buf_append(node->token.value);
            buf_append(" ");
            generateExpression(right, table);
            buf_append(")");
            break;
        }

        case AST_FUNCTION_CALL: {
            buf_append(node->token.value);
            buf_append("(");
            for (int i = 0; i < node->childCount; i++) {
                if (i > 0) buf_append(", ");
                generateExpression(node->children[i], table);
            }
            buf_append(")");
            break;
        }

        default:
            buf_append("NULL");
            break;
    }
}

// ============================================================
// generateGlobalDeclarations
// FIX #5: Only emit truly global / global-implicit variables.
// SCOPE_FILE_LOCAL vars are locals declared at the top level of the
// Lua file; they will be declared with their proper type inside
// main() by generateLocalAssign, so we skip them here.
// ============================================================
static void generateGlobalDeclarations(SymbolTable* globalTable) {
    if (!globalTable) return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = globalTable->buckets[i];
        while (entry != NULL) {
            SymbolRecord* rec = entry->record;

            // Skip functions — they get their own full definition
            if (rec->type == TYPE_FUNCTION) {
                entry = entry->next;
                continue;
            }

            // FIX #5: Skip file-local (top-level `local`) vars — they will be
            // declared inside main() without the redundant global forward-decl.
            if (rec->scope == SCOPE_FILE_LOCAL) {
                entry = entry->next;
                continue;
            }

            // SCOPE_GLOBAL or SCOPE_GLOBAL_IMPLICIT
            char line[256];
            snprintf(line, sizeof(line), "%s %s;\n", cTypeName(rec->type), rec->name);
            buf_append(line);

            entry = entry->next;
        }
    }
}

// ============================================================
// generateFunctions
// FIX #4: Skip a function named "main" — the Lua `function main()`
// body is emitted as the C main() body, not as a separate function.
// ============================================================
static void generateFunctions(ASTNode* root, SymbolTable* globalTable) {
    if (!root) return;

    for (int i = 0; i < root->childCount; i++) {
        ASTNode* node = root->children[i];
        if (!node || node->type != AST_FUNCTION_DECL) continue;

        // FIX #4: "main" is handled by the C main() wrapper — don't double-emit it.
        if (strcmp(node->token.value, "main") == 0) continue;

        // FIX #3: retrieve the function's own local scope from the semantic
        // analyser so that locals inside the function body get the right type.
        SymbolTable* funcScope = getFuncScope(node->token.value);
        SymbolTable* tableToUse = funcScope ? funcScope : globalTable;

        generateFunction(node, 0, tableToUse);
        buf_append("\n");
    }
}

// ============================================================
// generateBlock
// ============================================================
static void generateBlock(ASTNode* node, int indent, SymbolTable* table) {
    if (!node) return;

    for (int i = 0; i < node->childCount; i++) {
        ASTNode* child = node->children[i];
        if (!child) continue;

        switch (child->type) {
            case AST_ASSIGNMENT:
                generateAssign(child, indent, table);
                break;
            case AST_LOCAL_ASSIGN:
                generateLocalAssign(child, indent, table);
                break;
            case AST_IF:
                generateIf(child, indent, table);
                break;
            case AST_WHILE:
            case AST_REPEAT:
                generateLoop(child, indent, table);
                break;
            case AST_FOR:
                generateFor(child, indent, table);
                break;
            case AST_FUNCTION_DECL:
                // Nested function definition — generate inline.
                generateFunction(child, indent, table);
                buf_append("\n");
                break;
            case AST_FUNCTION_CALL:
                generateCall(child, indent, table);
                break;
            case AST_RETURN:
                generateReturn(child, indent, table);
                break;
            default:
                break;
        }
    }
}

// ============================================================
// generateAssign — x = expr  (variable was already declared globally)
// ============================================================
static void generateAssign(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);
    ASTNode* varNode = node->children[0];
    ASTNode* valNode = node->children[1];
    buf_append(varNode->token.value);
    buf_append(" = ");
    generateExpression(valNode, table);
    buf_append(";\n");
}

// ============================================================
// generateLocalAssign — local x = expr
// FIX #5: Inside main() FILE_LOCAL vars are emitted as plain locals
// (no `static`).  Inside a function body BLOCK_LOCAL vars are plain.
// ============================================================
static void generateLocalAssign(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);

    ASTNode*    varNode = node->children[0];
    ASTNode*    valNode = node->children[1];
    const char* name    = varNode->token.value;

    // FIX #3: look up the variable in the *current* scope table (which is the
    // function-local table when we're inside a function body) so that we get
    // the properly inferred type rather than falling back to void*.
    SymbolRecord* rec   = lookupSymbol(table, name);
    SymbolType    sType = rec ? rec->type : TYPE_UNKNOWN;
    const char*   cType = cTypeName(sType);

    // FIX #5: Never emit `static` inside a function body.  Top-level locals
    // were formerly emitted as `static int x` inside main() — they are now
    // just plain `int x`.
    buf_append(cType);
    buf_append(" ");
    buf_append(name);
    buf_append(" = ");
    generateExpression(valNode, table);
    buf_append(";\n");
}

// ============================================================
// generateIf — if / elseif / else
// Each branch body has its own child scope created during semantic
// analysis (in the exact order they appear in the source).  We call
// getNextChildScope(table) once per branch to follow that order.
// ============================================================
static void generateIf(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);
    buf_append("if (");
    generateExpression(node->children[0], table);
    buf_append(") {\n");

    // Consume the child scope for the if-body.
    SymbolTable* ifScope = getNextChildScope(table);
    generateBlock(node->children[1], indent + 1, ifScope ? ifScope : table);

    if (node->childCount > 2 && node->children[2] != NULL &&
        node->children[2]->type != AST_NIL) {

        ASTNode* elseNode = node->children[2];

        if (elseNode->type == AST_IF) {
            // Walk the whole elseif chain iteratively so every branch
            // consumes exactly one child scope in creation order.
            ASTNode* cur = elseNode;
            while (cur != NULL && cur->type == AST_IF) {
                write_indent(indent);
                buf_append("} else if (");
                generateExpression(cur->children[0], table);
                buf_append(") {\n");

                SymbolTable* branchScope = getNextChildScope(table);
                generateBlock(cur->children[1], indent + 1,
                              branchScope ? branchScope : table);

                if (cur->childCount > 2 && cur->children[2] != NULL &&
                    cur->children[2]->type != AST_NIL) {
                    cur = cur->children[2];
                } else {
                    cur = NULL;
                }
            }
            // cur is now a plain else block or NULL
            if (cur != NULL) {
                write_indent(indent);
                buf_append("} else {\n");
                SymbolTable* elseScope = getNextChildScope(table);
                generateBlock(cur, indent + 1, elseScope ? elseScope : table);
            }
        } else {
            // Plain else block
            write_indent(indent);
            buf_append("} else {\n");
            SymbolTable* elseScope = getNextChildScope(table);
            generateBlock(elseNode, indent + 1, elseScope ? elseScope : table);
        }
    }

    write_indent(indent);
    buf_append("}\n");
}

// ============================================================
// generateLoop — while / repeat-until
// ============================================================
static void generateLoop(ASTNode* node, int indent, SymbolTable* table) {
    // Each loop body got its own child scope during semantic analysis.
    SymbolTable* loopScope = getNextChildScope(table);
    SymbolTable* inner     = loopScope ? loopScope : table;

    if (node->type == AST_WHILE) {
        write_indent(indent);
        buf_append("while (");
        generateExpression(node->children[0], table);
        buf_append(") {\n");
        generateBlock(node->children[1], indent + 1, inner);
        write_indent(indent);
        buf_append("}\n");
    } else {
        // REPEAT / UNTIL → do { ... } while (!(condition));
        write_indent(indent);
        buf_append("do {\n");
        generateBlock(node->children[0], indent + 1, inner);
        write_indent(indent);
        buf_append("} while (!(");
        generateExpression(node->children[1], table);
        buf_append("));\n");
    }
}

// ============================================================
// generateFor — numeric for loop
// ============================================================
static void generateFor(ASTNode* node, int indent, SymbolTable* table) {
    ASTNode* varNode   = node->children[0];
    ASTNode* startNode = node->children[1];
    ASTNode* limitNode = node->children[2];
    ASTNode* stepNode  = node->children[3];
    ASTNode* bodyNode  = node->children[4];

    // Advance into the for-loop's child scope (holds the iterator variable).
    SymbolTable* forScope = getNextChildScope(table);
    SymbolTable* inner    = forScope ? forScope : table;

    SymbolRecord* rec   = lookupSymbol(inner, varNode->token.value);
    const char*   cType = (rec && rec->type != TYPE_UNKNOWN) ? cTypeName(rec->type) : "int";

    write_indent(indent);
    buf_append("for (");
    buf_append(cType);
    buf_append(" ");
    buf_append(varNode->token.value);
    buf_append(" = ");
    generateExpression(startNode, table);
    buf_append("; ");
    buf_append(varNode->token.value);
    buf_append(" <= ");
    generateExpression(limitNode, table);
    buf_append("; ");
    buf_append(varNode->token.value);
    buf_append(" += ");
    generateExpression(stepNode, table);
    buf_append(") {\n");

    generateBlock(bodyNode, indent + 1, inner);

    buf_append("}\n");
}

// ============================================================
// generateFunction — function declaration
// FIX #3: The caller (generateFunctions) passes the function-local
// SymbolTable so locals inside the body resolve correctly.
// ============================================================
static void generateFunction(ASTNode* node, int indent, SymbolTable* table) {
    const char* funcName   = node->token.value;
    int         paramCount = node->childCount - 1;
    ASTNode*    bodyNode   = node->children[node->childCount - 1];

    // Look up the function record in whichever table was passed in (which
    // may be the function-local scope whose parent is the global table).
    SymbolRecord* funcRec    = lookupSymbol(table, funcName);
    SymbolType    returnType = (funcRec && funcRec->type == TYPE_FUNCTION)
                               ? funcRec->data.func_data.return_type
                               : TYPE_UNKNOWN;
    const char*   retCType   = cTypeName(returnType);

    write_indent(indent);
    buf_append(retCType);
    buf_append(" ");
    buf_append(funcName);
    buf_append("(");

    for (int i = 0; i < paramCount; i++) {
        if (i > 0) buf_append(", ");
        const char* paramName = node->children[i]->token.value;
        SymbolType  paramType = TYPE_UNKNOWN;
        if (funcRec && funcRec->type == TYPE_FUNCTION &&
            i < funcRec->data.func_data.params.param_count) {
            paramType = funcRec->data.func_data.params.param_types[i];
        }
        buf_append(cTypeName(paramType));
        buf_append(" ");
        buf_append(paramName);
    }

    buf_append(") {\n");
    generateBlock(bodyNode, indent + 1, table);
    write_indent(indent);
    buf_append("}\n");
}

// ============================================================
// generateCall — function call statement
// ============================================================
static void generateCall(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);
    const char* funcName = node->token.value;

    // Special case: Lua print() → printf
    if (strcmp(funcName, "print") == 0) {
        if (node->childCount == 0) {
            buf_append("printf(\"\\n\");\n");
            return;
        }
        ASTNode* arg = node->children[0];
        SymbolType argType = TYPE_UNKNOWN;
        if (arg->type == AST_NUMBER) {
            argType = strchr(arg->token.value, '.') ? TYPE_DOUBLE : TYPE_INT;
        } else if (arg->type == AST_STRING) {
            argType = TYPE_STRING;
        } else if (arg->type == AST_IDENTIFIER) {
            if (strcmp(arg->token.value, "true")  == 0 ||
                strcmp(arg->token.value, "false") == 0) {
                argType = TYPE_BOOL;
            } else {
                SymbolRecord* rec = lookupSymbol(table, arg->token.value);
                if (rec) argType = rec->type;
            }
        } else if (arg->type == AST_FUNCTION_CALL) {
            SymbolRecord* rec = lookupSymbol(table, arg->token.value);
            if (rec && rec->type == TYPE_FUNCTION)
                argType = rec->data.func_data.return_type;
        }
        if (argType == TYPE_UNKNOWN) {
            buf_append("printf(\"nil\\n\");\n");
            return;
        }
        char fmt[64];
        snprintf(fmt, sizeof(fmt), "printf(\"%s\\n\", ", printfFmt(argType));
        buf_append(fmt);
        generateExpression(arg, table);
        buf_append(");\n");
        return;
    }

    // Regular call
    buf_append(funcName);
    buf_append("(");
    for (int i = 0; i < node->childCount; i++) {
        if (i > 0) buf_append(", ");
        generateExpression(node->children[i], table);
    }
    buf_append(");\n");
}

// ============================================================
// generateReturn
// ============================================================
static void generateReturn(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);
    buf_append("return ");
    if (node->childCount > 0 && node->children[0]->type != AST_NIL) {
        generateExpression(node->children[0], table);
    }
    buf_append(";\n");
}

// ============================================================
// generateCode — main entry point
// ============================================================

// Helper: does the AST have a top-level function named "main"?
static int hasLuaMainFunc(ASTNode* root) {
    if (!root) return 0;
    for (int i = 0; i < root->childCount; i++) {
        ASTNode* c = root->children[i];
        if (c && c->type == AST_FUNCTION_DECL &&
            strcmp(c->token.value, "main") == 0) return 1;
    }
    return 0;
}

// Helper: find the Lua `function main()` node.
static ASTNode* findLuaMainNode(ASTNode* root) {
    if (!root) return NULL;
    for (int i = 0; i < root->childCount; i++) {
        ASTNode* c = root->children[i];
        if (c && c->type == AST_FUNCTION_DECL &&
            strcmp(c->token.value, "main") == 0) return c;
    }
    return NULL;
}

void generateCode(ASTNode* root, SymbolTable* globalTable, const char* outputFilename) {
    buf_init();

    // Standard headers
    buf_append("#include <stdio.h>\n");
    buf_append("#include <stdlib.h>\n");
    buf_append("#include <string.h>\n");
    buf_append("#include <stdbool.h>\n");
    buf_append("\n");

    // Lua string concatenation helper
    buf_append("// Lua string concatenation helper\n");
    buf_append("static char* concat_strings(const char* a, const char* b) {\n");
    buf_append("    char* result = (char*)malloc(strlen(a) + strlen(b) + 1);\n");
    buf_append("    strcpy(result, a);\n");
    buf_append("    strcat(result, b);\n");
    buf_append("    return result;\n");
    buf_append("}\n\n");

    // Global variable forward declarations (truly global vars only — FIX #5)
    generateGlobalDeclarations(globalTable);
    buf_append("\n");

    // All non-main function definitions (FIX #4: main skipped here)
    generateFunctions(root, globalTable);

    // ============================================================
    // C main()
    // FIX #4: If the Lua file has a `function main()`, its body
    // becomes the C main body directly (no wrapper call, no duplicate).
    // Otherwise, the top-level Lua statements become the C main body.
    // ============================================================
    buf_append("int main() {\n");

    if (hasLuaMainFunc(root)) {
        // Emit the body of Lua's `function main()` as the C main body.
        ASTNode*     luaMain   = findLuaMainNode(root);
        ASTNode*     bodyNode  = luaMain->children[luaMain->childCount - 1];
        SymbolTable* mainScope = getFuncScope("main");
        SymbolTable* tableToUse = mainScope ? mainScope : globalTable;

        // Also emit top-level statements that are NOT function declarations
        // and NOT `function main` (e.g. top-level `local rot = nil`).
        for (int i = 0; i < root->childCount; i++) {
            ASTNode* child = root->children[i];
            if (!child) continue;
            if (child->type == AST_FUNCTION_DECL) continue; // all funcs already emitted above

            switch (child->type) {
                case AST_ASSIGNMENT:   generateAssign(child, 1, globalTable);      break;
                case AST_LOCAL_ASSIGN: generateLocalAssign(child, 1, globalTable); break;
                case AST_IF:           generateIf(child, 1, globalTable);          break;
                case AST_WHILE:
                case AST_REPEAT:       generateLoop(child, 1, globalTable);        break;
                case AST_FOR:          generateFor(child, 1, globalTable);         break;
                case AST_FUNCTION_CALL:generateCall(child, 1, globalTable);        break;
                case AST_RETURN:       generateReturn(child, 1, globalTable);      break;
                default: break;
            }
        }

        // Now emit the body of Lua `function main()` directly
        generateBlock(bodyNode, 1, tableToUse);

    } else {
        // No Lua `function main()` — top-level statements become C main body.
        if (root) {
            for (int i = 0; i < root->childCount; i++) {
                ASTNode* child = root->children[i];
                if (!child || child->type == AST_FUNCTION_DECL) continue;

                switch (child->type) {
                    case AST_ASSIGNMENT:   generateAssign(child, 1, globalTable);      break;
                    case AST_LOCAL_ASSIGN: generateLocalAssign(child, 1, globalTable); break;
                    case AST_IF:           generateIf(child, 1, globalTable);          break;
                    case AST_WHILE:
                    case AST_REPEAT:       generateLoop(child, 1, globalTable);        break;
                    case AST_FOR:          generateFor(child, 1, globalTable);         break;
                    case AST_FUNCTION_CALL:generateCall(child, 1, globalTable);        break;
                    case AST_RETURN:       generateReturn(child, 1, globalTable);      break;
                    default: break;
                }
            }
        }
    }

    buf_append("    return 0;\n");
    buf_append("}\n");

    // Write output file
    FILE* outFile = fopen(outputFilename, "w");
    if (!outFile) {
        printf("Code Generation Error: Cannot open '%s' for writing\n", outputFilename);
        free(g_buf.data);
        return;
    }
    fputs(g_buf.data, outFile);
    fclose(outFile);
    printf("      Written to '%s' (%d bytes)\n", outputFilename, g_buf.length);
    free(g_buf.data);
}