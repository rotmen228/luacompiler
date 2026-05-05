#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexerH.h"
#include "error_handler.h"

// חישוב אוטומטי של גודל מילון מילות המפתח
#define KEYWORD_DICT_SIZE (sizeof(keyword_dict) / sizeof(keyword_dict[0]))
// חישוב אוטומטי של גודל מילון האופרטורים
#define OPERATOR_DICT_SIZE (sizeof(operator_dict) / sizeof(operator_dict[0]))

// פונקציית עזר: הקצאת זיכרון לרשימה
static void initTokenList(TokenList* list) {
    list->capacity = 100;
    list->count = 0;
    list->tokens = (Token*)malloc(list->capacity * sizeof(Token));
}

// פונקציית עזר: הוספת אסימון לרשימה
static void addToken(TokenList* list, TokenType type, const char* value, int line) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->tokens = (Token*)realloc(list->tokens, list->capacity * sizeof(Token));
    }
    list->tokens[list->count].type = type;
    list->tokens[list->count].value = value ? strdup(value) : NULL;
    list->tokens[list->count].line = line;
    list->count++;
}

// פונקציה לבדיקת מילות מפתח
static TokenType identifyKeywordOrId(const char* word) {
    // ריצה על מילון מילות המפתח ושליפה
    for (int i = 0; i < KEYWORD_DICT_SIZE; i++) {
        if (strcmp(word, keyword_dict[i].text) == 0) {
            return keyword_dict[i].type; // מצאנו מילת מפתח!
        }
    }
    
    // אם הסתיימה הלולאה ולא מצאנו - זה חייב להיות שם משתנה/פונקציה
    return TOKEN_IDENTIFIER; 
}

// פונקציה לבדיקת אופרטורים
static TokenType identifyOperator(const char* op) {
    for (int i = 0; i < OPERATOR_DICT_SIZE; i++) {
        if (strcmp(op, operator_dict[i].text) == 0) {
            return operator_dict[i].type;
        }
    }
    return TOKEN_ERROR; 
}

// הפונקציה הראשית: ריצת האוטומט
TokenList runLexer(const char* sourceCode) {
    TokenList list;
    initTokenList(&list);
    
    int currentLine = 1;
    const char* ptr = sourceCode;
    
    while (*ptr != '\0') {
        if (isspace(*ptr)) {
            if (*ptr == '\n') currentLine++;
            ptr++;
            continue;
        }
        
        //דילוג על הערות)
        if (*ptr == '-' && *(ptr+1) == '-') {
            while (*ptr != '\n' && *ptr != '\0') ptr++;
            continue;
        }
        
        // התחלת קריאת אסימון חדש - מנגנון Maximal Munch
        const char* startPos = ptr;
        LexerState state = STATE_START;
        char buffer[256] = {0};
        int bufIdx = 0;
        
        while (state != STATE_ERROR && *ptr != '\0') {
            char c = *ptr;
            
            switch (state) {
                case STATE_START:
                    if (isalpha(c) || c == '_') {
                        state = STATE_IN_ID;
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else if (isdigit(c)) {
                        state = STATE_NUMBER;
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else if (c == '"' || c == '\'') {
                        state = STATE_STRING;
                        ptr++; // מדלגים על המרכאות
                    } else if (strchr("+-*/=~<>.(),:%", c)) {
                        state = STATE_OPERATOR;
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        state = STATE_ERROR; // תו לא חוקי
                        buffer[bufIdx++] = c;
                        ptr++;
                    }
                    break;
                    
                case STATE_IN_ID:
                    if (isalnum(c) || c == '_') {
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        state = STATE_ERROR;
                    }
                    break;
                    
                case STATE_NUMBER:
                    if (isdigit(c) || c == '.') {
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        state = STATE_ERROR;
                    }
                    break;
                    
                case STATE_OPERATOR:
                    if ((buffer[0] == '=' && c == '=') || 
                        (buffer[0] == '~' && c == '=') ||
                        (buffer[0] == '<' && c == '=') ||
                        (buffer[0] == '>' && c == '=') ||
                        (buffer[0] == '.' && c == '.')) {
                        buffer[bufIdx++] = c;
                        ptr++;
                    }
                    state = STATE_ERROR;
                    break;
                    
                case STATE_STRING:
                    if (c != '"' && c != '\'') {
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        ptr++;
                        state = STATE_ERROR;
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        // Tokenize
        buffer[bufIdx] = '\0';
        
        if (startPos[0] == '"' || startPos[0] == '\'') {
            addToken(&list, TOKEN_STRING, buffer, currentLine);
        }
        else if (isalpha(buffer[0]) || buffer[0] == '_') {
            TokenType type = identifyKeywordOrId(buffer);
            addToken(&list, type, buffer, currentLine);
        } 
        else if (isdigit(buffer[0])) {
            addToken(&list, TOKEN_NUMBER, buffer, currentLine);
        }
        else {
            TokenType type = identifyOperator(buffer);
            if (type != TOKEN_ERROR) {
                addToken(&list, type, buffer, currentLine);
            } else {
                reportError(PHASE_LEXICAL, currentLine, "Unknown character '%s'", buffer);
            }
        }
    }
    addToken(&list, TOKEN_EOF, "EOF", currentLine);
    return list;
}

// שחרור זיכרון
void freeTokenList(TokenList* list) {
    for (int i = 0; i < list->count; i++) {
        if (list->tokens[i].value) {
            free(list->tokens[i].value);
        }
    }
    free(list->tokens);
}