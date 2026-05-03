#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"

// ============================================================
// Output buffer — grows dynamically, holds the whole C file
// ============================================================
typedef struct {
    char*  data;
    int    length;
    int    capacity;
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
static void generateFunctions  (ASTNode* root,  SymbolTable* table);

// ============================================================
// Helpers
// ============================================================

// Build an indentation string of (indent * 4) spaces
static void write_indent(int indent) {
    char spaces[128] = {0};
    int  n = indent * 4;
    if (n > 127) n = 127;
    for (int i = 0; i < n; i++) spaces[i] = ' ';
    buf_append(spaces);
}

// Map a SymbolType to the C type string
static const char* cTypeName(SymbolType type) {
    switch (type) {
        case TYPE_INT:    return "int";
        case TYPE_DOUBLE: return "double";
        case TYPE_STRING: return "char*";
        case TYPE_BOOL:   return "int";   // Lua booleans → C int (0/1)
        case TYPE_VOID:   return "void";
        default:          return "void*"; // TYPE_UNKNOWN → void* (safe fallback)
    }
}

// Map a SymbolType to the correct printf format specifier
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
// generateExpression — Post-order DFS on an expression node
// ============================================================
static void generateExpression(ASTNode* node, SymbolTable* table) {
    if (!node) { buf_append("NULL"); return; }

    switch (node->type) {

        // Leaf: nil
        case AST_NIL:
            buf_append("NULL");
            break;

        // Leaf: number or string — emit the raw token value
        case AST_NUMBER:
        case AST_STRING:
            buf_append("\"");
            buf_append(node->token.value);
            buf_append("\"");
            break;

        // Leaf: true / false / identifier
        case AST_IDENTIFIER:
            if (strcmp(node->token.value, "true")  == 0) { buf_append("1"); break; }
            if (strcmp(node->token.value, "false") == 0) { buf_append("0"); break; }
            buf_append(node->token.value);
            break;

        // Binary operation — Post-order: left, right, then operator (root)
        case AST_BINOP: {
            ASTNode* left  = node->children[0];
            ASTNode* right = node->children[1];

            // Special case: Lua string concat (..) → strcat(left, right)
            if (node->token.type == TOKEN_OP_CONCAT) {
                buf_append("concat_strings(");
                generateExpression(left,  table);
                buf_append(", ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // Special case: Lua not-equal (~=) → (left != right)
            if (node->token.type == TOKEN_OP_NEQ) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" != ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }
            if (node->token.type == TOKEN_KW_AND) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" && ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }
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

        // Inline function call inside an expression (e.g. x = foo(1))
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
// Walks the global symbol table and emits one C declaration per
// variable, respecting SCOPE_FILE_LOCAL (static) vs global.
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

            const char* cType = cTypeName(rec->type);

            // Select if UNKNOWN → void* (safe)
            // Select if SCOPE_FILE_LOCAL → add static keyword
            if (rec->scope == SCOPE_FILE_LOCAL) {
                char line[256];
                snprintf(line, sizeof(line), "static %s %s;\n", cType, rec->name);
                buf_append(line);
            } else {
                // SCOPE_GLOBAL or SCOPE_GLOBAL_IMPLICIT
                char line[256];
                snprintf(line, sizeof(line), "%s %s;\n", cType, rec->name);
                buf_append(line);
            }

            entry = entry->next;
        }
    }
}

// ============================================================
// generateFunctions — forward-declares and defines all
// AST_FUNCTION_DECL nodes found at the top level, so they
// appear before main().
// ============================================================
static void generateFunctions(ASTNode* root, SymbolTable* table) {
    if (!root) return;
    

    for (int i = 0; i < root->childCount; i++) {
        ASTNode* node = root->children[i];
        if (node && node->type == AST_FUNCTION_DECL) {
            generateFunction(node, 0, table);
            buf_append("\n");
        }
    }
}

// ============================================================
// generateBlock — iterates children of an AST_BLOCK node
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
                // Function definitions inside a block are rare in Lua but valid;
                // generate them inline at the current indent level.
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
// generateAssign — x = expr
// Parser layout: children[0]=IDENTIFIER, children[1]=value
// The variable was already declared globally — just assign.
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
// Parser layout: children[0]=IDENTIFIER, children[1]=value
// Scope comes from the symbol table to decide "static" or plain.
// ============================================================
static void generateLocalAssign(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);

    ASTNode*      varNode = node->children[0];
    ASTNode*      valNode = node->children[1];
    const char*   name    = varNode->token.value;

    // Look up the type from the symbol table
    SymbolRecord* rec    = lookupSymbol(table, name);
    SymbolType    sType  = rec ? rec->type : TYPE_UNKNOWN;
    ScopeType     scope  = rec ? rec->scope : SCOPE_BLOCK_LOCAL;
    const char*   cType  = cTypeName(sType);

    // Emit the right prefix based on scope
    if (scope == SCOPE_FILE_LOCAL) {
        buf_append("static ");
        buf_append(cType);
    } else {
        // SCOPE_BLOCK_LOCAL — plain type
        buf_append(cType);
    }

    buf_append(" ");
    buf_append(name);
    buf_append(" = ");
    generateExpression(valNode, table);
    buf_append(";\n");
}

// ============================================================
// generateIf — if / elseif / else
// Parser layout: children[0]=condition, children[1]=body BLOCK,
//               children[2]=else body or elseif AST_IF (optional)
// ============================================================
static void generateIf(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);
    buf_append("if (");
    generateExpression(node->children[0], table);
    buf_append(") {\n");

    // Body block (children[1])
    generateBlock(node->children[1], indent + 1, table);

    // Optional else / elseif (children[2])
    if (node->childCount > 2 && node->children[2] != NULL &&
        node->children[2]->type != AST_NIL) {

        ASTNode* elseNode = node->children[2];

        if (elseNode->type == AST_IF) {
            // elseif — emit "} else if (...) {"
            write_indent(indent);
            buf_append("} else ");
            // Re-use generateIf but strip the leading indent (already written)
            // We call it with indent to get the closing brace right
            buf_append("if (");
            generateExpression(elseNode->children[0], table);
            buf_append(") {\n");
            generateBlock(elseNode->children[1], indent + 1, table);

            // Recurse for further elseif/else chains
            if (elseNode->childCount > 2 && elseNode->children[2] != NULL &&
                elseNode->children[2]->type != AST_NIL) {
                // Let the closing brace + else be handled below
                write_indent(indent);
                buf_append("} else {\n");
                generateBlock(elseNode->children[2], indent + 1, table);
            }
        } else {
            // Plain else block
            write_indent(indent);
            buf_append("} else {\n");
            generateBlock(elseNode, indent + 1, table);
        }
    }

    write_indent(indent);
    buf_append("}\n");
}

// ============================================================
// generateLoop — while / repeat-until
// AST_WHILE: children[0]=condition, children[1]=body
// AST_REPEAT: children[0]=body,     children[1]=condition
// ============================================================
static void generateLoop(ASTNode* node, int indent, SymbolTable* table) {
    if (node->type == AST_WHILE) {
        write_indent(indent);
        buf_append("while (");
        generateExpression(node->children[0], table);
        buf_append(") {\n");
        generateBlock(node->children[1], indent + 1, table);
        write_indent(indent);
        buf_append("}\n");

    } else {
        // REPEAT / UNTIL → C do { ... } while (!(condition));
        write_indent(indent);
        buf_append("do {\n");
        generateBlock(node->children[0], indent + 1, table);
        write_indent(indent);
        buf_append("} while (!(");
        generateExpression(node->children[1], table);
        buf_append("));\n");
    }
}

// ============================================================
// generateFor — numeric for loop
// Parser layout: [0]=iter_var [1]=start [2]=limit [3]=step [4]=body
// ============================================================
static void generateFor(ASTNode* node, int indent, SymbolTable* table) {
    // Always 5 children (parser always inserts a default step of 1)
    ASTNode* varNode  = node->children[0];
    ASTNode* startNode = node->children[1];
    ASTNode* limitNode = node->children[2];
    ASTNode* stepNode  = node->children[3];
    ASTNode* bodyNode  = node->children[4];

    // Determine the iterator variable's C type from symbol table
    SymbolRecord* rec   = lookupSymbol(table, varNode->token.value);
    const char*   cType = rec ? cTypeName(rec->type) : "int";

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

    generateBlock(bodyNode, indent + 1, table);

    write_indent(indent);
    buf_append("}\n");
}

// ============================================================
// generateFunction — function declaration
// Parser layout: token=function_name,
//   children[0..n-2] = parameter IDENTIFIER nodes,
//   children[n-1]    = body BLOCK
// ============================================================
static void generateFunction(ASTNode* node, int indent, SymbolTable* table) {
    const char* funcName  = node->token.value;
    int         paramCount = node->childCount - 1;
    ASTNode*    bodyNode   = node->children[node->childCount - 1];

    // Look up the function's return type in the symbol table
    SymbolRecord* funcRec    = lookupSymbol(table, funcName);
    SymbolType    returnType = (funcRec && funcRec->type == TYPE_FUNCTION)
                               ? funcRec->data.func_data.return_type
                               : TYPE_UNKNOWN;
    const char*   retCType   = cTypeName(returnType);

    write_indent(indent);

    // Emit: returnType funcName(param1_type param1, ...)
    buf_append(retCType);
    buf_append(" ");
    buf_append(funcName);
    buf_append("(");

    for (int i = 0; i < paramCount; i++) {
        if (i > 0) buf_append(", ");

        const char* paramName = node->children[i]->token.value;

        // Look up the parameter's inferred type
        SymbolType paramType = TYPE_UNKNOWN;
        if (funcRec && funcRec->type == TYPE_FUNCTION &&
            i < funcRec->data.func_data.params.param_count) {
            paramType = funcRec->data.func_data.params.param_types[i];
        }
        buf_append(cTypeName(paramType));
        buf_append(" ");
        buf_append(paramName);
    }

    buf_append(") {\n");

    // Body
    generateBlock(bodyNode, indent + 1, table);

    write_indent(indent);
    buf_append("}\n");
}

// ============================================================
// generateCall — function call as a statement
// Special handling for Lua's built-in print()
// Parser layout: token=func_name, children = arguments
// ============================================================
static void generateCall(ASTNode* node, int indent, SymbolTable* table) {
    write_indent(indent);

    const char* funcName = node->token.value;

    // --- Special case: Lua print() → printf ---
    if (strcmp(funcName, "print") == 0) {
        if (node->childCount == 0) {
            buf_append("printf(\"\\n\");\n");
            return;
        }

        ASTNode* arg = node->children[0];

        // Determine the type of the argument to pick the right format specifier
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

        // Build printf call with the correct format specifier
        char fmt[64];
        if (argType == TYPE_UNKNOWN) {
            // Fallback: print as nil
            buf_append("printf(\"nil\\n\");\n");
            return;
        }
        snprintf(fmt, sizeof(fmt), "printf(\"%s\\n\", ", printfFmt(argType));
        buf_append(fmt);
        generateExpression(arg, table);
        buf_append(");\n");
        return;
    }

    // --- Regular function call ---
    buf_append(funcName);
    buf_append("(");
    for (int i = 0; i < node->childCount; i++) {
        if (i > 0) buf_append(", ");
        generateExpression(node->children[i], table);
    }
    buf_append(");\n");
}

// ============================================================
// generateReturn — return statement
// Parser layout: children[0] = return value (or AST_NIL)
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
void generateCode(ASTNode* root, SymbolTable* globalTable, const char* outputFilename) {
    buf_init();

    // Step 2: standard headers
    buf_append("#include <stdio.h>\n");
    buf_append("#include <stdlib.h>\n");
    buf_append("#include <string.h>\n");
    buf_append("#include <stdbool.h>\n");
    buf_append("\n");

    // Helper for Lua string concatenation (..)
    buf_append("// Lua string concatenation helper\n");
    buf_append("static char* concat_strings(const char* a, const char* b) {\n");
    buf_append("    char* result = (char*)malloc(strlen(a) + strlen(b) + 1);\n");
    buf_append("    strcpy(result, a);\n");
    buf_append("    strcat(result, b);\n");
    buf_append("    return result;\n");
    buf_append("}\n\n");

    // Step 3: global variable declarations
    generateGlobalDeclarations(globalTable);
    buf_append("\n");

    // Step 4: function definitions (before main)
    generateFunctions(root, globalTable);

    // Step 5: open main
    buf_append("int main() {\n");

    // Step 6: generate all top-level statements (skip function defs — already emitted)
    if (root) {
        for (int i = 0; i < root->childCount; i++) {
            ASTNode* child = root->children[i];
            if (!child || child->type == AST_FUNCTION_DECL) continue;

            switch (child->type) {
                case AST_ASSIGNMENT:
                    generateAssign(child, 1, globalTable);
                    break;
                case AST_LOCAL_ASSIGN:
                    generateLocalAssign(child, 1, globalTable);
                    break;
                case AST_IF:
                    generateIf(child, 1, globalTable);
                    break;
                case AST_WHILE:
                case AST_REPEAT:
                    generateLoop(child, 1, globalTable);
                    break;
                case AST_FOR:
                    generateFor(child, 1, globalTable);
                    break;
                case AST_FUNCTION_CALL:
                    generateCall(child, 1, globalTable);
                    break;
                case AST_RETURN:
                    generateReturn(child, 1, globalTable);
                    break;
                default:
                    break;
            }
        }
    }

    // Step 7: close main
    buf_append("    return 0;\n");
    buf_append("}\n");

    // Step 8: write to the specified output file
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
