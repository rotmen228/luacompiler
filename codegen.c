#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "error_handler.h"

static OutputBuffer g_buf;

static void buf_init(void) {
    g_buf.capacity = 4096;
    g_buf.length = 0;
    g_buf.data = (char*)malloc(g_buf.capacity);
    g_buf.data[0] = '\0';
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
        case TYPE_BOOL:   return "int";//bool -> 0/1
        case TYPE_VOID:   return "void";
        default:          return "void*";
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
// Type-detection helpers used by generateExpression
// ============================================================

static int isNumericNode(ASTNode* n, SymbolTable* table) {
    if (!n) return 0;
    if (n->type == AST_NUMBER) return 1;
    if (n->type == AST_IDENTIFIER) {
        SymbolRecord* r = lookupSymbol(table, n->token.value);
        return (r && (r->type == TYPE_INT || r->type == TYPE_DOUBLE));
    }
    return 0;
}

//returns 1 if the node definitely yields a C char*
static int isStringNode(ASTNode* n, SymbolTable* table) {
    if (!n) return 0;
    if (n->type == AST_STRING) return 1;
    if (n->type == AST_BINOP && n->token.type == TOKEN_OP_CONCAT) return 1;
    if (n->type == AST_IDENTIFIER) {
        SymbolRecord* r = lookupSymbol(table, n->token.value);
        return (r && (r->type == TYPE_STRING || r->type == TYPE_UNKNOWN));
    }
    if (n->type == AST_FUNCTION_CALL) {
        SymbolRecord* r = lookupSymbol(table, n->token.value);
        return (r && r->type == TYPE_FUNCTION &&
                (r->data.func_data.return_type == TYPE_STRING ||
                 r->data.func_data.return_type == TYPE_UNKNOWN));
    }
    return 0;
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

        case AST_NUMBER:
            buf_append(node->token.value);
            break;

        case AST_STRING:
            buf_append("\"");
            buf_append(node->token.value);
            buf_append("\"");
            break;

        case AST_IDENTIFIER:
            if (strcmp(node->token.value, "true") == 0) { buf_append("1"); break; }
            if (strcmp(node->token.value, "false") == 0) { buf_append("0"); break; }
            buf_append(node->token.value);
            break;

        case AST_BINOP: {
            ASTNode* left  = node->children[0];
            ASTNode* right = node->children[1];

            if (node->token.type == TOKEN_OP_CONCAT) {
                int leftIsNum = isNumericNode(left,  table);
                int rightIsNum = isNumericNode(right, table);
                buf_append("concat_strings(");
                if (leftIsNum)  { buf_append("num_to_str("); generateExpression(left,  table); buf_append(")"); }
                else generateExpression(left,  table);
                buf_append(", ");
                if (rightIsNum) { buf_append("num_to_str("); generateExpression(right, table); buf_append(")"); }
                else generateExpression(right, table);
                buf_append(")");
                break;
            }

            // Lua == / ~= on strings is str_eq()
            // NULL comparisons (against nil) keep the plain == / != 
            if (node->token.type == TOKEN_OP_EQ || node->token.type == TOKEN_OP_NEQ) {
                int needStrEq = (isStringNode(left, table) || isStringNode(right, table))
                                && left->type  != AST_NIL
                                && right->type != AST_NIL;
                if (needStrEq) {
                    if (node->token.type == TOKEN_OP_NEQ) buf_append("(!");
                    buf_append("str_eq(");
                    generateExpression(left,  table);
                    buf_append(", ");
                    generateExpression(right, table);
                    buf_append(")");
                    if (node->token.type == TOKEN_OP_NEQ) buf_append(")");
                } else {
                    buf_append("(");
                    generateExpression(left, table);
                    buf_append(node->token.type == TOKEN_OP_NEQ ? " != " : " == ");
                    generateExpression(right, table);
                    buf_append(")");
                }
                break;
            }

            // Lua amd is && in C
            if (node->token.type == TOKEN_KW_AND) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" && ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // Lua or is || in C
            if (node->token.type == TOKEN_KW_OR) {
                buf_append("(");
                generateExpression(left, table);
                buf_append(" || ");
                generateExpression(right, table);
                buf_append(")");
                break;
            }

            // All other operators
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
// emit global / global-implicit variables.
// ============================================================
static void generateGlobalDeclarations(SymbolTable* globalTable) {
    if (!globalTable) return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = globalTable->buckets[i];
        while (entry != NULL) {
            SymbolRecord* rec = entry->record;

            //skip functions - they get their own full definition
            if (rec->type == TYPE_FUNCTION) {
                entry = entry->next;
                continue;
            }

            //skip file-local vars - they will be
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
// generateFunctions — emit all top-level function definitions.
// Note: the Lua source must not define a function named "main"
// as that name is reserved for the generated C entry point.
// ============================================================
static void generateFunctions(ASTNode* root, SymbolTable* globalTable) {
    if (!root) return;

    for (int i = 0; i < root->childCount; i++) {
        ASTNode* node = root->children[i];
        if (!node || node->type != AST_FUNCTION_DECL) continue;

        SymbolTable* funcScope  = getFuncScope(node->token.value);
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
//
// Shadow-safety fix: In Lua, `local x = x * 10` means "read the
// *outer* x, then create a new x".  In C, `int x = x * 10` is UB
// because the RHS sees the new (uninitialised) x.
// Solution: when the variable name already exists in an *outer*
// scope (parent or higher), evaluate the RHS into a uniquely-named
// temp first, then declare the shadow from the temp.
//
//   int __tmp_shadow_current_val = (current_val * 10);
//   int current_val = __tmp_shadow_current_val;
//
// When there is no shadowing the old single-line form is used.
// ============================================================

// Counter for unique temp names (reset per generateCode call via buf_init path)
static int g_shadow_tmp_counter = 0;

static void generateLocalAssign(ASTNode* node, int indent, SymbolTable* table) {
    ASTNode*    varNode = node->children[0];
    ASTNode*    valNode = node->children[1];
    const char* name    = varNode->token.value;

    // Look up type in the current (innermost) scope table.
    SymbolRecord* rec   = lookupSymbol(table, name);
    SymbolType    sType = rec ? rec->type : TYPE_UNKNOWN;
    const char*   cType = cTypeName(sType);

    // Detect shadowing: does this name exist in a *parent* scope?
    // (The current table was just entered, so any record in table->parent_table
    //  or higher is the outer binding that the RHS should see.)
    int isShadow = 0;
    if (table->parent_table != NULL) {
        SymbolRecord* outer = lookupSymbol(table->parent_table, name);
        if (outer != NULL) {
            isShadow = 1;
        }
    }

    if (isShadow) {
        // Step 1: evaluate RHS into a temp (still sees the outer `name`)
        char tmpName[128];
        snprintf(tmpName, sizeof(tmpName), "__tmp_shadow_%s_%d", name, g_shadow_tmp_counter++);

        write_indent(indent);
        buf_append(cType);
        buf_append(" ");
        buf_append(tmpName);
        buf_append(" = ");
        generateExpression(valNode, table);
        buf_append(";\n");

        // Step 2: declare the shadow from the temp
        write_indent(indent);
        buf_append(cType);
        buf_append(" ");
        buf_append(name);
        buf_append(" = ");
        buf_append(tmpName);
        buf_append(";\n");
    } else {
        // No shadowing — simple single-line form
        write_indent(indent);
        buf_append(cType);
        buf_append(" ");
        buf_append(name);
        buf_append(" = ");
        generateExpression(valNode, table);
        buf_append(";\n");
    }
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
// Top-level Lua statements become the C main() body.
// A top-level `return <call>()` (the Lua entry-point idiom,
// e.g. `return mainLua()`) is emitted as a plain call so the
// program runs it and then falls through to `return 0`.
// ============================================================

static int isEntryPointReturn(ASTNode* node) {
    return node->type == AST_RETURN &&
           node->childCount > 0 &&
           node->children[0] != NULL &&
           node->children[0]->type == AST_FUNCTION_CALL;
}

void generateCode(ASTNode* root, SymbolTable* globalTable, const char* outputFilename) {
    buf_init();
    g_shadow_tmp_counter = 0;  // reset per compilation unit

    buf_append("#include <stdio.h>\n");
    buf_append("#include <stdlib.h>\n");
    buf_append("#include <string.h>\n");
    buf_append("#include <stdbool.h>\n");
    buf_append("\n");

    buf_append("// Lua string concatenation helper\n");
    buf_append("static char* concat_strings(const char* a, const char* b) {\n");
    buf_append("    if (!a) a = \"nil\"; if (!b) b = \"nil\";\n");
    buf_append("    char* result = (char*)malloc(strlen(a) + strlen(b) + 1);\n");
    buf_append("    strcpy(result, a);\n");
    buf_append("    strcat(result, b);\n");
    buf_append("    return result;\n");
    buf_append("}\n\n");

    buf_append("// Lua number-to-string coercion helper (for .. with numbers)\n");
    buf_append("static char* num_to_str(double v) {\n");
    buf_append("    char* buf = (char*)malloc(64);\n");
    buf_append("    if (v == (long long)v) snprintf(buf, 64, \"%lld\", (long long)v);\n");
    buf_append("    else snprintf(buf, 64, \"%g\", v);\n");
    buf_append("    return buf;\n");
    buf_append("}\n\n");

    buf_append("// Lua string equality helper (strcmp wrapper)\n");
    buf_append("static int str_eq(const char* a, const char* b) {\n");
    buf_append("    if (!a && !b) return 1;\n");
    buf_append("    if (!a || !b) return 0;\n");
    buf_append("    return strcmp(a, b) == 0;\n");
    buf_append("}\n\n");

    generateGlobalDeclarations(globalTable);
    buf_append("\n");

    generateFunctions(root, globalTable);

    buf_append("int main() {\n");

    if (root) {
        for (int i = 0; i < root->childCount; i++) {
            ASTNode* child = root->children[i];
            if (!child || child->type == AST_FUNCTION_DECL) continue;

            if (isEntryPointReturn(child)) {
                // `return foo()` → plain call, then fall through to return 0
                generateCall(child->children[0], 1, globalTable);
                continue;
            }

            switch (child->type) {
                case AST_ASSIGNMENT:    generateAssign(child, 1, globalTable);      break;
                case AST_LOCAL_ASSIGN:  generateLocalAssign(child, 1, globalTable); break;
                case AST_IF:            generateIf(child, 1, globalTable);          break;
                case AST_WHILE:
                case AST_REPEAT:        generateLoop(child, 1, globalTable);        break;
                case AST_FOR:           generateFor(child, 1, globalTable);         break;
                case AST_FUNCTION_CALL: generateCall(child, 1, globalTable);        break;
                case AST_RETURN:        generateReturn(child, 1, globalTable);      break;
                default: break;
            }
        }
    }

    buf_append("    return 0;\n");
    buf_append("}\n");

    FILE* outFile = fopen(outputFilename, "w");
    if (!outFile) {
        reportError(PHASE_CODEGEN, 0, "Cannot open '%s' for writing", outputFilename);
        free(g_buf.data);
        return;
    }
    fputs(g_buf.data, outFile);
    fclose(outFile);
    printf("      Written to '%s' (%d bytes)\n", outputFilename, g_buf.length);
    free(g_buf.data);
}