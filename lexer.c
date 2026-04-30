#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexerH.h"

// מצבי האוטומט הסופי (מתוך המודל שלך)
typedef enum {
    STATE_START,
    STATE_IN_ID,
    STATE_NUMBER,
    STATE_OPERATOR,
    STATE_STRING,
    STATE_ERROR
} LexerState;

// מבנה המייצג כניסה במילון (מחרוזת -> סוג אסימון)
typedef struct {
    const char* text;
    TokenType type;
} TokenDict;

// מילון מילות המפתח (Keywords)
static const TokenDict keyword_dict[] = {
    {"and",      TOKEN_KW_AND},
    {"do",       TOKEN_KW_DO},
    {"else",     TOKEN_KW_ELSE},
    {"elseif",   TOKEN_KW_ELSEIF},
    {"end",      TOKEN_KW_END},
    {"false",    TOKEN_KW_FALSE},
    {"for",      TOKEN_KW_FOR},
    {"function", TOKEN_KW_FUNCTION},
    {"if",       TOKEN_KW_IF},
    {"local",    TOKEN_KW_LOCAL},
    {"nil",      TOKEN_KW_NIL},
    {"not",      TOKEN_KW_NOT},
    {"or",       TOKEN_KW_OR},
    {"repeat",   TOKEN_KW_REPEAT},
    {"return",   TOKEN_KW_RETURN},
    {"then",     TOKEN_KW_THEN},
    {"true",     TOKEN_KW_TRUE},
    {"until",    TOKEN_KW_UNTIL},
    {"while",    TOKEN_KW_WHILE}
};

// חישוב אוטומטי של גודל מילון מילות המפתח
#define KEYWORD_DICT_SIZE (sizeof(keyword_dict) / sizeof(keyword_dict[0]))

// מילון האופרטורים והסימנים (Operators & Punctuation)
static const TokenDict operator_dict[] = {
    {"==", TOKEN_OP_EQ},
    {"~=", TOKEN_OP_NEQ},
    {"<=", TOKEN_OP_LTE},
    {">=", TOKEN_OP_GTE},
    {"..", TOKEN_OP_CONCAT},
    {"+",  TOKEN_OP_PLUS},
    {"-",  TOKEN_OP_MINUS},
    {"*",  TOKEN_OP_MUL},
    {"/",  TOKEN_OP_DIV},
    {"<",  TOKEN_OP_LT},
    {">",  TOKEN_OP_GT},
    {"=",  TOKEN_OP_ASSIGN},
    {"(",  TOKEN_PUNC_LPAREN},
    {")",  TOKEN_PUNC_RPAREN},
    {",",  TOKEN_PUNC_COMMA}
};

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
    // ריצה על מילון האופרטורים
    for (int i = 0; i < OPERATOR_DICT_SIZE; i++) {
        if (strcmp(op, operator_dict[i].text) == 0) {
            return operator_dict[i].type; // מצאנו אופרטור מוכר!
        }
    }
    
    // אם הגענו לפה, יש לנו בעיה לקסיקלית
    return TOKEN_ERROR; 
}

// הפונקציה הראשית: ריצת האוטומט
TokenList runLexer(const char* sourceCode) {
    TokenList list;
    initTokenList(&list);
    
    int currentLine = 1;
    const char* ptr = sourceCode;
    
    while (*ptr != '\0') {
        // שלב 2.1 (עמוד 57): דילוג על רווחים וירידות שורה
        if (isspace(*ptr)) {
            if (*ptr == '\n') currentLine++;
            ptr++;
            continue;
        }
        
        // דילוג על הערות (מתחילות ב -- בלואה)
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
                    } else if (strchr("+-*/=~<>.(),:", c)) {
                        state = STATE_OPERATOR;
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        state = STATE_ERROR; // תו לא חוקי
                    }
                    break;
                    
                case STATE_IN_ID:
                    if (isalnum(c) || c == '_') {
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        state = STATE_ERROR; // התיקון: עוצרים אך לא מקדמים את ה-PTR!
                    }
                    break;
                    
                case STATE_NUMBER:
                    // תומך במספרים עשרוניים
                    if (isdigit(c) || c == '.') {
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        state = STATE_ERROR;
                    }
                    break;
                    
                case STATE_OPERATOR:
                    // טיפול באופרטורים של שני תווים כמו ==, ~=, <=, >=, ..
                    if ((buffer[0] == '=' && c == '=') || 
                        (buffer[0] == '~' && c == '=') ||
                        (buffer[0] == '<' && c == '=') ||
                        (buffer[0] == '>' && c == '=') ||
                        (buffer[0] == '.' && c == '.')) {
                        buffer[bufIdx++] = c;
                        ptr++;
                    }
                    state = STATE_ERROR; // הגענו לסוף האופרטור, חותכים!
                    break;
                    
                case STATE_STRING:
                    // ממשיכים לקרוא עד המרכאות הסוגרות
                    if (c != '"' && c != '\'') {
                        buffer[bufIdx++] = c;
                        ptr++;
                    } else {
                        ptr++; // מדלגים על המרכאות הסוגרות
                        state = STATE_ERROR; // מסיימים את הקריאה
                    }
                    break;
                    
                default:
                    break;
            }
        }
        
        // יצאנו ממצב קריאה - שלב ה-Tokenize (עמוד 58)
        buffer[bufIdx] = '\0';
        
        // כאן זיהינו אילו תווים נחתכו, ועכשיו מאפיינים אותם לפי הקבוצה
        if (isalpha(buffer[0]) || buffer[0] == '_') {
            TokenType type = identifyKeywordOrId(buffer);
            addToken(&list, type, buffer, currentLine);
        } 
        else if (isdigit(buffer[0])) {
            addToken(&list, TOKEN_NUMBER, buffer, currentLine);
        } 
        else if (startPos[0] == '"' || startPos[0] == '\'') {
            addToken(&list, TOKEN_STRING, buffer, currentLine);
        }
        else {
            TokenType type = identifyOperator(buffer);
            if (type != TOKEN_ERROR) {
                addToken(&list, type, buffer, currentLine);
            } else {
                printf("Lexical Error at line %d: Unknown character '%s'\n", currentLine, buffer);
            }
        }
    }
    
    // סימן סוף קובץ
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