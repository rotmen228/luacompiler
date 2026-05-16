#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "semantic.h"
#include "error_handler.h"

//tracks the specific function we are currently analyzing to verify the returns statements type
static SymbolRecord* currentFunctionScope = NULL;

//arrays to allow a quick search of functions and its parameters without searching the entire AST
static SymbolTable* allScopes[100]; //pointers to the symbol tables
static char allScopeNames[100][128]; //the names of those functions
static char allFuncParamNames[100][20][64]; //params names
static int scopeCount = 0; //how many functions we have registered so far

//searches the flat registry to find the symbol table belonging to a function
SymbolTable* getFuncScope(const char* funcName) {
    for (int i = 0; i < scopeCount; i++) {
        if (strcmp(allScopeNames[i], funcName) == 0) {
            return allScopes[i];
        }
    }
    return NULL;
}

// ==========================================
// debug / print helpers
// ==========================================
static const char* getSymbolTypeName(SymbolType type) {
    switch(type) {
        case TYPE_INT: return "int";
        case TYPE_DOUBLE: return "double";
        case TYPE_STRING: return "string";
        case TYPE_BOOL: return "bool";
        case TYPE_FUNCTION: return "function";
        case TYPE_VOID: return "void";
        case TYPE_UNKNOWN: return "UNKNOWN";
        default: return "???";
    }
}

static const char* getScopeTypeName(ScopeType scope) {
    switch(scope) {
        case SCOPE_GLOBAL:          return "GLOBAL";
        case SCOPE_GLOBAL_IMPLICIT: return "GLOBAL_IMPLICIT";
        case SCOPE_FILE_LOCAL:      return "FILE_LOCAL";
        case SCOPE_BLOCK_LOCAL:     return "BLOCK_LOCAL";
        default:                    return "???";
    }
}

void printSymbolTable(SymbolTable* table, const char* scopeName) {
    if (!table) return;

    printf("\n=== Symbol Table: %s ===\n", scopeName);
    printf("%-15s | %-10s | %-15s | %s\n", "Name", "Type", "Scope", "Initialized?");
    printf("----------------------------------------------------------\n");
    bool isEmpty = true;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = table->buckets[i];
        while (entry != NULL) {
            isEmpty = false;
            SymbolRecord* rec = entry->record;
            const char* initStr = (rec->type == TYPE_FUNCTION) ? "N/A" : (rec->data.var_data.is_initialized ? "Yes" : "No");
            printf("%-15s | %-10s | %-15s | %s\n", rec->name, getSymbolTypeName(rec->type), getScopeTypeName(rec->scope), initStr);
            entry = entry->next;
        }
    }
    if (isEmpty) printf("(Empty Table)\n");
    printf("==========================================================\n\n");
}

void printFinalSymbolTables(SymbolTable* globalScope) {
    printf("\n=== FINAL SYMBOL TABLES ===\n");
    for (int i = 0; i < scopeCount; i++) {
        printSymbolTable(allScopes[i], allScopeNames[i]);
    }
    if (globalScope != NULL) {
        printSymbolTable(globalScope, "Global Scope");
    }
}

// ==========================================
// Hash table helpers
// ==========================================

static unsigned int hash(const char* str) {
    unsigned int h = 5381;
    int c;
    while ((c = *str++)) h = ((h << 5) + h) + c;
    return h % HASH_TABLE_SIZE;
}

SymbolTable* createSymbolTable(SymbolTable* parent) {
    SymbolTable* table = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (!table) { 
        printf("Memory allocation failed for SymbolTable\n"); 
        exit(1); 
    }
    for (int i = 0; i < HASH_TABLE_SIZE; i++) table->buckets[i] = NULL;
    table->parent_table = parent;
    table->children = NULL;
    table->childCount = 0;
    table->childCapacity = 0;
    table->nextChild = 0;
    if (parent != NULL) {
        if (parent->childCount >= parent->childCapacity) {
            int newCap = (parent->childCapacity == 0) ? 4 : parent->childCapacity * 2;
            parent->children = (SymbolTable**)realloc(parent->children, newCap * sizeof(SymbolTable*));
            parent->childCapacity = newCap;
        }
        parent->children[parent->childCount++] = table;
    }
    return table;
}

SymbolRecord* createVarRecord(const char* name, SymbolType type, ScopeType scope, bool is_init) {
    SymbolRecord* record = (SymbolRecord*)malloc(sizeof(SymbolRecord));
    record->name  = strdup(name);
    record->type  = type;
    record->scope = scope;
    record->data.var_data.is_initialized = is_init;
    return record;
}

SymbolRecord* createFuncRecord(const char* name, SymbolType returnType, SymbolType* paramTypes, int paramCount) {
    SymbolRecord* record = (SymbolRecord*)malloc(sizeof(SymbolRecord));
    record->name = strdup(name);
    record->type = TYPE_FUNCTION;
    record->scope = SCOPE_GLOBAL;
    record->data.func_data.return_type = returnType;
    record->data.func_data.params.param_types = paramTypes;
    record->data.func_data.params.param_count = paramCount;
    return record;
}

void insertSymbol(SymbolTable* table, SymbolRecord* record) {
    unsigned int idx = hash(record->name) % HASH_TABLE_SIZE;
    //duplicate declaration check within the same scope
    for (HashEntry* e = table->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->record->name, record->name) == 0) {
            reportError(PHASE_SEMANTIC, 0, "Duplicate declaration of '%s' in the same scope", record->name);
            return; // dont insert the shadowing entry
        }
    }

    HashEntry* entry = malloc(sizeof(HashEntry));
    entry->record = record;
    entry->next = table->buckets[idx];
    table->buckets[idx] = entry;
}

SymbolRecord* lookupSymbol(SymbolTable* table, const char* name) {
    SymbolTable* current = table;
    while (current != NULL) {
        unsigned int index = hash(name);
        HashEntry* entry = current->buckets[index];
        while (entry != NULL) {
            if (strcmp(entry->record->name, name) == 0) return entry->record;
            entry = entry->next;
        }
        current = current->parent_table;
    }
    return NULL;
}

//returns the next child scope of table in creation order, advancing the curser
SymbolTable* getNextChildScope(SymbolTable* table) {
    if (!table || table->nextChild >= table->childCount) return NULL;
    return table->children[table->nextChild++];
}

static SymbolTable* getGlobalTable(SymbolTable* table) {
    SymbolTable* current = table;
    while (current->parent_table != NULL) current = current->parent_table;
    return current;
}

static bool isGlobalTable(SymbolTable* table) {
    return table->parent_table == NULL;
}

// ==========================================
// type inference
// ==========================================
//recursively evaluates an AST node to determine what data type it evaluates to
SymbolType inferType(ASTNode* node, SymbolTable* table) {
    if (!node)
    {
        return TYPE_UNKNOWN;
    }
    switch (node->type) {
        //primitive types
        case AST_NUMBER:
            return strchr(node->token.value, '.') ? TYPE_DOUBLE : TYPE_INT;

        case AST_STRING:
            return TYPE_STRING;

        case AST_NIL:
            return TYPE_UNKNOWN;
        //variables and bool
        case AST_IDENTIFIER: {
            if (strcmp(node->token.value, "true") == 0 || strcmp(node->token.value, "false") == 0) return TYPE_BOOL;
            //var
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record)
            {
                return record->type;
            }
            reportError(PHASE_SEMANTIC, node->token.line, "Use of undeclared variable '%s'", node->token.value);
            return TYPE_UNKNOWN;
        }
        //infer form expression
        case AST_BINOP: {
            SymbolType leftType = inferType(node->children[0], table);
            SymbolType rightType = inferType(node->children[1], table);
            TokenType op = node->token.type;
            //math or comparison
            if (op == TOKEN_OP_PLUS || op == TOKEN_OP_MINUS || op == TOKEN_OP_MUL || op == TOKEN_OP_DIV || op == TOKEN_OP_MOD || op == TOKEN_OP_POW || op == TOKEN_OP_LT || op == TOKEN_OP_GT || op == TOKEN_OP_LTE || op == TOKEN_OP_GTE) {
                //type inference of left based on right type in case its still not identified
                if (leftType == TYPE_UNKNOWN && node->children[0]->type == AST_IDENTIFIER) {
                    SymbolRecord* rec = lookupSymbol(table, node->children[0]->token.value);
                    if (rec) {
                        //modulo is integers only, if right is double so is left
                        SymbolType g = (op == TOKEN_OP_MOD) ? TYPE_INT : (rightType == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
                        rec->type = g;
                        leftType = g;
                    }
                }
                //same thing on right side if unk
                if (rightType == TYPE_UNKNOWN && node->children[1]->type == AST_IDENTIFIER) {
                    SymbolRecord* rec = lookupSymbol(table, node->children[1]->token.value);
                    if (rec) {
                        SymbolType g = (op == TOKEN_OP_MOD) ? TYPE_INT : (leftType == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
                        rec->type = g;
                        rightType = g;
                    }
                }
            }
            //check if the two stypes can be used in the context of the operator
            return checkTypeCompatibility(leftType, op, rightType, node->token.line);
        }
        //return type of a call
        case AST_FUNCTION_CALL: {
            //verify the arguments passed to the function
            analyzeSemanticCall(node, table);

            if (strcmp(node->token.value, "print") == 0) {
                return TYPE_VOID;
            }
            //get return type
            SymbolRecord* record = lookupSymbol(table, node->token.value);
            if (record && record->type == TYPE_FUNCTION)
                return record->data.func_data.return_type;
            return TYPE_UNKNOWN;
        }

        default:
            return TYPE_UNKNOWN;
    }
}

// ==========================================
// type compatibility
// ==========================================
//validates that two data types can legally interact using the given operator and returns the combination
static SymbolType checkTypeCompatibility(SymbolType left, TokenType op, SymbolType right, int line) {
    switch (op) {
        //comparison op
        case TOKEN_OP_EQ:
        case TOKEN_OP_NEQ:
        case TOKEN_OP_LT:
        case TOKEN_OP_GT:
        case TOKEN_OP_LTE:
        case TOKEN_OP_GTE:
            //if both types are known, ensure we arent mixing strings with another type
            if (left != TYPE_UNKNOWN && right != TYPE_UNKNOWN) {
                if ((left == TYPE_STRING) != (right == TYPE_STRING)) {
                    reportError(PHASE_SEMANTIC, line, "Cannot compare string with non-string");
                }
            }
            //comparisons always a boolean
            return TYPE_BOOL;

        //logical op
        case TOKEN_KW_AND:
        case TOKEN_KW_OR:
            //logical operations always a boolean
            return TYPE_BOOL;

        //string op (..)
        case TOKEN_OP_CONCAT: {
            //lua allows numbers to be implicitly converted to strings during concatenation
            bool leftValid  = (left == TYPE_STRING || left == TYPE_INT || left == TYPE_DOUBLE || left == TYPE_UNKNOWN);
            bool rightValid = (right == TYPE_STRING || right == TYPE_INT || right == TYPE_DOUBLE || right == TYPE_UNKNOWN);

            if (!leftValid || !rightValid) {
                reportError(PHASE_SEMANTIC, line, "Concatenation (..) requires operands to be strings or numbers");
                return TYPE_UNKNOWN;
            }
            //always a string
            return TYPE_STRING;
        }

        //math op
        default: {
            //math operations strictly require numbers
            bool leftValid  = (left  == TYPE_INT || left  == TYPE_DOUBLE || left  == TYPE_UNKNOWN);
            bool rightValid = (right == TYPE_INT || right == TYPE_DOUBLE || right == TYPE_UNKNOWN);
            
            if (!leftValid || !rightValid) {
                reportError(PHASE_SEMANTIC, line, "Cannot perform math operation on non-numeric types");
                return TYPE_UNKNOWN;
            }
            
            //if we have double, answer is double, else int
            if (left == TYPE_DOUBLE || right == TYPE_DOUBLE) return TYPE_DOUBLE;
            return TYPE_INT;
        }
    }
}

// ==========================================
// statement analyzers
// ==========================================
static void analyzeSemanticAssign(ASTNode* node, SymbolTable* table) {
    //variable
    ASTNode* varNode = node->children[0];
    //value
    ASTNode* valNode = node->children[1];
    const char* varName = varNode->token.value;

    SymbolType inferred = inferType(valNode, table);
    SymbolRecord* existing = lookupSymbol(table, varName);
    //if already exists, check if value assignment is legal
    if (existing != NULL) {
        if (existing->type == TYPE_UNKNOWN) {
            existing->type = inferred;
            existing->data.var_data.is_initialized = true;
        //mismatch
        } else if (existing->type != inferred && inferred != TYPE_UNKNOWN) {
            reportError(PHASE_SEMANTIC, node->token.line, "Type mismatch for variable '%s'", varName);
        }
    }
    //create the record
    else {
        //scope
        ScopeType scope;
        SymbolTable* targetTable;
        if (isGlobalTable(table)) {
            scope = SCOPE_GLOBAL;
            targetTable = table;
        } else {
            scope = SCOPE_GLOBAL_IMPLICIT;
            targetTable = getGlobalTable(table);
        }
        //add record
        SymbolRecord* newRecord = createVarRecord(varName, inferred, scope, true);
        insertSymbol(targetTable, newRecord);
    }
}

static void analyzeSemanticLocal(ASTNode* node, SymbolTable* table) {
    //variable
    ASTNode* varNode = node->children[0];
    //value
    ASTNode* valNode = node->children[1];
    const char* varName = varNode->token.value;

    SymbolType inferred = TYPE_UNKNOWN;
    bool initialized = false;
    //is initialized
    if (valNode != NULL && valNode->type != AST_NIL) {
        inferred = inferType(valNode, table);
        initialized = true;
    }
    //add record if its initialized or just declated. local x
    ScopeType scope = isGlobalTable(table) ? SCOPE_FILE_LOCAL : SCOPE_BLOCK_LOCAL;
    SymbolRecord* newRecord = createVarRecord(varName, inferred, scope, initialized);
    insertSymbol(table, newRecord);
}

static void analyzeSemanticIf(ASTNode* node, SymbolTable* table) {
    ASTNode* condNode = node->children[0];
    ASTNode* bodyNode = node->children[1];
    ASTNode* elseNode = (node->childCount > 2) ? node->children[2] : NULL;

    //condition expression must be int or bool
    SymbolType condType = inferType(condNode, table);
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN)
        reportError(PHASE_SEMANTIC, node->token.line, "if condition must be boolean or numeric");

    //create the if body scope and analyze it
    SymbolTable* ifScope = createSymbolTable(table);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, ifScope);

    //handle the else/elseif branch
    if (elseNode != NULL && elseNode->type != AST_NIL) {
        if (elseNode->type == AST_IF) {
            //elseif recurse, it will create its own scope internally
            analyzeSemanticIf(elseNode, table);
        } else {
            //plain else block, create one scope and analyze it
            SymbolTable* elseScope = createSymbolTable(table);
            analyzeSemanticBlock(elseNode->children, elseNode->childCount, elseScope);
        }
    }
}

static void analyzeSemanticLoop(ASTNode* node, SymbolTable* table) {
    //handle while and repeat loops
    ASTNode* condNode;
    ASTNode* bodyNode;
    if (node->type == AST_WHILE) {
        condNode = node->children[0];
        bodyNode = node->children[1];
    } else {
        bodyNode = node->children[0];
        condNode = node->children[1];
    }

    SymbolType condType = inferType(condNode, table);
    //condition expression must be int or bool
    if (condType != TYPE_BOOL && condType != TYPE_INT && condType != TYPE_UNKNOWN)
        reportError(PHASE_SEMANTIC, node->token.line, "Loop condition must be boolean or numeric");
    SymbolTable* loopScope = createSymbolTable(table);
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, loopScope);
}

static void analyzeSemanticFor(ASTNode* node, SymbolTable* table) {
    ASTNode* varNode   = node->children[0];
    ASTNode* startNode = node->children[1];
    ASTNode* limitNode = node->children[2];
    //step is optional
    ASTNode* stepNode  = (node->childCount == 5) ? node->children[3] : NULL;
    ASTNode* bodyNode  = node->children[node->childCount - 1];

    //type inference
    SymbolType startType = inferType(startNode, table);
    SymbolType limitType = inferType(limitNode, table);
    SymbolType stepType  = (stepNode != NULL) ? inferType(stepNode, table) : TYPE_INT;

    //must be a number
    bool startOk = (startType == TYPE_INT || startType == TYPE_DOUBLE || startType == TYPE_UNKNOWN);
    bool limitOk = (limitType == TYPE_INT || limitType == TYPE_DOUBLE || limitType == TYPE_UNKNOWN);
    bool stepOk  = (stepType  == TYPE_INT || stepType  == TYPE_DOUBLE || stepType  == TYPE_UNKNOWN);
    //error if not
    if (!startOk || !limitOk || !stepOk)
        reportError(PHASE_SEMANTIC, node->token.line, "for loop bounds must be numeric");
    //create scope
    SymbolTable* forScope = createSymbolTable(table);
    //if srart or limit is double then the iterable must be as well
    SymbolType iterType = (startType == TYPE_DOUBLE || limitType == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
    SymbolRecord* iterRecord = createVarRecord(varNode->token.value, iterType, SCOPE_BLOCK_LOCAL, true);
    insertSymbol(forScope, iterRecord);
    //analize block
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, forScope);
}

static void analyzeSemanticFunction(ASTNode* node, SymbolTable* table) {
    const char* funcName = node->token.value;
    ASTNode* bodyNode = node->children[node->childCount - 1];
    int paramCount = node->childCount - 1;
    //allocate space for the params and initialize thier type to unknown
    SymbolType* paramTypes = NULL;
    if (paramCount > 0) {
        paramTypes = (SymbolType*)malloc(sizeof(SymbolType) * paramCount);
        for (int i = 0; i < paramCount; i++) paramTypes[i] = TYPE_UNKNOWN;
    }
    //create function record
    SymbolRecord* funcRecord = createFuncRecord(funcName, TYPE_UNKNOWN, paramTypes, paramCount);
    insertSymbol(table, funcRecord);
    //create scope
    SymbolTable* funcScope = createSymbolTable(table);
    //populate the params in the scope
    for (int i = 0; i < paramCount; i++) {
        const char* paramName = node->children[i]->token.value;
        SymbolRecord* paramRecord = createVarRecord(paramName, TYPE_UNKNOWN, SCOPE_BLOCK_LOCAL, true);
        insertSymbol(funcScope, paramRecord);
    }
    //analize the function scope we created, and then restore the global scope to the previous one
    SymbolRecord* prevFunc = currentFunctionScope;
    currentFunctionScope = funcRecord;
    analyzeSemanticBlock(bodyNode->children, bodyNode->childCount, funcScope);
    currentFunctionScope = prevFunc;

    //update inferred param types from the function scope to its parents function record
    if (paramCount > 0 && paramTypes != NULL) {
        for (int i = 0; i < paramCount; i++) {
            const char* paramName = node->children[i]->token.value;
            SymbolRecord* paramRecord = lookupSymbol(funcScope, paramName);
            if (paramRecord) funcRecord->data.func_data.params.param_types[i] = paramRecord->type;
        }
    }

    // Save scope for codegen
    if (scopeCount < 100) {
        allScopes[scopeCount] = funcScope;
        strncpy(allScopeNames[scopeCount], funcName, 127);
        allScopeNames[scopeCount][127] = '\0';
        for (int i = 0; i < paramCount && i < 20; i++) {
            strncpy(allFuncParamNames[scopeCount][i], node->children[i]->token.value, 63);
            allFuncParamNames[scopeCount][i][63] = '\0';
        }
        scopeCount++;
    }
}

static void analyzeSemanticReturn(ASTNode* node, SymbolTable* table) {
    //empty returns, void function
    if (node->childCount == 0 || node->children[0] == NULL) {
        if (currentFunctionScope != NULL) {
            if (currentFunctionScope->data.func_data.return_type == TYPE_UNKNOWN) {
                currentFunctionScope->data.func_data.return_type = TYPE_VOID;
            //if there is a return that returned something and now we have one that returns nothing, ist a contradiction 
            } else if (currentFunctionScope->data.func_data.return_type != TYPE_VOID) {
                //we previously returned a value, but now we are returning nothing, error
                reportError(PHASE_SEMANTIC, node->token.line, "Function '%s' returns both void and non-void values", currentFunctionScope->name);
            }
        }
        return;
    }
    //infer the return type since its not void
    SymbolType returnType = inferType(node->children[0], table);
    
    if (currentFunctionScope != NULL) {
        SymbolType cur = currentFunctionScope->data.func_data.return_type;
        //see if there are contredicting return types already
        switch (cur) {
            //first return weve faced
            case TYPE_UNKNOWN:
                currentFunctionScope->data.func_data.return_type = returnType;
                break;
            //previously returned an integer
            case TYPE_INT:
                //can be double, just need to change the return to double
                if (returnType == TYPE_DOUBLE) {
                    currentFunctionScope->data.func_data.return_type = TYPE_DOUBLE;
                }
                //if not int or unk as well then error
                else if (returnType != TYPE_INT && returnType != TYPE_UNKNOWN) {
                    reportError(PHASE_SEMANTIC, node->token.line, "Inconsistent return types in function '%s'", currentFunctionScope->name);
                }
                break;

            //return double in the past
            case TYPE_DOUBLE:
                //error if not int double or unk
                if (returnType != TYPE_INT && returnType != TYPE_DOUBLE && returnType != TYPE_UNKNOWN) {
                    reportError(PHASE_SEMANTIC, node->token.line, "Inconsistent return types in function '%s'", currentFunctionScope->name);
                }
                break;

            //strings, booleans, void
            default:
                //MUST match
                if (cur != returnType && returnType != TYPE_UNKNOWN) {
                    reportError(PHASE_SEMANTIC, node->token.line, "Inconsistent return types in function '%s'", currentFunctionScope->name);
                }
                break;
        }
    }
}

static void analyzeSemanticCall(ASTNode* node, SymbolTable* table) {
    const char* funcName = node->token.value;

    //check print
    if (strcmp(funcName, "print") == 0) {
        for (int i = 0; i < node->childCount; i++) {
            inferType(node->children[i], table);
        }
    } 
    else {
        //lookup in current table
        SymbolRecord* funcRecord = lookupSymbol(table, funcName);

        //logic and type checks
        if (funcRecord == NULL) {
            reportError(PHASE_SEMANTIC, node->token.line, "Call to undefined function '%s'", funcName);
        } else if (funcRecord->type != TYPE_FUNCTION) {
            reportError(PHASE_SEMANTIC, node->token.line, "'%s' is not a function", funcName);
        } else {
            //verify the correct number of arguments was passed
            int expectedParams = funcRecord->data.func_data.params.param_count;
            int givenArgs = node->childCount;
            if (givenArgs != expectedParams) {
                reportError(PHASE_SEMANTIC, node->token.line, "Function '%s' expects %d arguments but got %d", funcName, expectedParams, givenArgs);
            } else {
                //argument Type checking & inference
                for (int i = 0; i < givenArgs; i++) {
                    //infer type from call. func(1, "hi"), infer child 1 is int and 2 is string
                    SymbolType argType = inferType(node->children[i], table);
                    //known types
                    SymbolType expectedType = funcRecord->data.func_data.params.param_types[i];
                    //if not known, its the infered
                    if (expectedType == TYPE_UNKNOWN) {
                        funcRecord->data.func_data.params.param_types[i] = argType;
                    
                        //look up the functions internal symbol table to update the parameters record there too
                        for (int j = 0; j < scopeCount; j++) {
                            if (strcmp(allScopeNames[j], funcName) == 0) {
                                //found, now update infered type
                                const char* paramName = allFuncParamNames[j][i];
                                SymbolRecord* paramRec = lookupSymbol(allScopes[j], paramName);
                                if (paramRec) {
                                    paramRec->type = argType;
                                }
                                break;
                            }
                        }
                    } else if (argType != expectedType && argType != TYPE_UNKNOWN) {
                        //inconsistency in expected and infered, error
                        reportError(PHASE_SEMANTIC, node->token.line, "Argument %d to function '%s' has wrong type", i + 1, funcName);
                    }
                }
            }
        }
    }
}

static void analyzeSemanticBlock(ASTNode** nodes, int count, SymbolTable* table) {
    for (int i = 0; i < count; i++) {
        ASTNode* node = nodes[i];
        if (node){
            switch (node->type) {
            case AST_ASSIGNMENT: analyzeSemanticAssign(node, table); break;
            case AST_LOCAL_ASSIGN: analyzeSemanticLocal(node, table); break;
            case AST_IF: analyzeSemanticIf(node, table); break;
            case AST_WHILE:
            case AST_REPEAT: analyzeSemanticLoop(node, table); break;
            case AST_FUNCTION_DECL: analyzeSemanticFunction(node, table); break;
            case AST_FOR: analyzeSemanticFor(node, table); break;
            case AST_FUNCTION_CALL: analyzeSemanticCall(node, table); break;
            case AST_RETURN: analyzeSemanticReturn(node, table); break;
            default: break;
            }
        }
    }
}

//debug
SymbolTable* analyzeSemantic(ASTNode* root) {
    if (!root) return NULL;
    scopeCount = 0;
    memset(allScopes, 0, sizeof(allScopes));
    memset(allScopeNames, 0, sizeof(allScopeNames));
    memset(allFuncParamNames, 0, sizeof(allFuncParamNames));
    SymbolTable* globalTable = createSymbolTable(NULL);
    printf("\n--- Starting Semantic Analysis ---\n");
    analyzeSemanticBlock(root->children, root->childCount, globalTable);
    printf("--- Semantic Analysis completed successfully! ---\n");
    return globalTable;
}



//cleanup helper
void freeSymbolTable(SymbolTable* table) {
    if (!table) return;

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashEntry* entry = table->buckets[i];
        while (entry) {
            HashEntry* next = entry->next;
            if (entry->record) {
                // Free paramTypes for function records
                if (entry->record->type == TYPE_FUNCTION &&
                    entry->record->data.func_data.params.param_types) {
                    free(entry->record->data.func_data.params.param_types);
                }
                free(entry->record->name);
                free(entry->record);
            }
            free(entry);
            entry = next;
        }
    }

    // Recurse into children
    for (int i = 0; i < table->childCount; i++) {
        freeSymbolTable(table->children[i]);
    }
    free(table->children);
    free(table);
}