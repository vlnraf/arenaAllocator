#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <cstring>

//--------------------------------------------------- Arena library --------------------------------------------------------------
#define DEFAULT_ALIGNMENT (2*sizeof(void *))
#define KB 1024
#define MB KB * 1024
#define GB MB * 1024
//#define KB(x) ((uint64_t)(x) * 1024)
//#define MB(x) KB(x) * 1024
//#define GB(x) MB(x) * 1024
#define DEFAULT_SIZE 4 * MB

struct Arena{
    void* memory;
    uint64_t index;
    uint64_t previousIndex;
    uint64_t size;
};

Arena* initArena(uint64_t memorySize = DEFAULT_SIZE){
    Arena* arena = (Arena*) malloc(sizeof(Arena));
    arena->memory = malloc(memorySize);
    arena->index = 0;
    arena->previousIndex = 0;
    arena->size = memorySize;
    return arena;
}


void clearArena(Arena* arena){
    arena->index = 0;
    arena->previousIndex = 0;
}

void destroyArena(Arena* arena){
    free(arena->memory);
    free(arena);
    arena->memory = NULL;
}

void* arenaAllocAligned(Arena* arena, uint64_t size, uint32_t align){
    uintptr_t currentAddr = (uintptr_t)arena->memory + arena->index;
    uintptr_t alignedAddr = (currentAddr + (align - 1)) & ~(uint64_t)(align - 1);
    uint64_t padding = alignedAddr - currentAddr;
    if(arena->index + padding + size > arena->size){
        return NULL;
    }

    arena->index += padding + size;
    return (void*)alignedAddr;
}

void* arenaAlloc(Arena* arena, uint64_t size){
    return arenaAllocAligned(arena, size, DEFAULT_ALIGNMENT);
}

void* arenaAllocAlignedZero(Arena* arena, uint64_t size, uint32_t align){
    void* result = (void*)arenaAllocAligned(arena, size, align);
    if(result){
        memset(result, 0, size);
    }
    return result;
}

void* arenaAllocZero(Arena* arena, uint64_t size){
    return arenaAllocAlignedZero(arena, size, DEFAULT_ALIGNMENT);
}

#define arenaAllocStruct(arena, T) (T*)arenaAlloc(arena, sizeof(T))
#define arenaAllocArray(arena, T, count) (T*)arenaAlloc(arena, sizeof(T) * count)

uint64_t arenaGetPos(Arena* arena){
    return arena->index;
}

uint64_t arenaGetMemoryUsed(Arena* arena){
    return arena->index;
}

uint64_t arenaGetMemoryLeft(Arena* arena){
    return arena->size - arena->index;
}

//--------------------------------------------------- String library --------------------------------------------------------------

struct String{
    char* text;
    uint32_t length;
};

String string(char* text, uint32_t size){
    String result;
    result.text = text;
    result.length = size;
    return result;
}

String pushString(Arena* arena, char* text, uint32_t size){
    String result;
    result.text = (char*)arenaAllocArray(arena, char, size);
    snprintf(result.text, size, "%s", text);
    result.length = size-1;
    return result;
}

String cStringToString(char* text, uint32_t size){
    String result;
    for(size_t i = 0; i < size; i++){
        if(text[i] == '\0'){
            result = string(text, i);
            break;
        }
    }
    return result;
}

#define string(text) string((char*)text, sizeof(text) - 1)
#define stringCArr(text) cStringToString((char*)text, sizeof(text) - 1)
#define pushString(arena, text) pushString(arena, (char*)text, sizeof(text))

bool charIsDigit(char c){
    if((uint8_t)c >= 48 && (uint8_t)c <= 57){
        return true;
    }
    return false;
}

uint32_t stringToUint(String s){
    uint32_t result = 0;
    for(size_t i = 0; i < s.length; i++){
        char c = s.text[i];
        if(charIsDigit(c)){
            result = result * 10 + (c - '0');
        }else{
            return 0;
        }
    }
    return result;
}

int stringToInt(String s){
    int result = 0;
    int sign = 0;
    bool skipFirst = false;
    if(s.text[0] == '-'){
        sign = -1;
        skipFirst = true;
    }else{
        sign = 1;
    }

    for(size_t i = 0; i < s.length; i++){
        if(skipFirst){
            i++;
            skipFirst = false;
        }
        char c = s.text[i];
        if(charIsDigit(c)){
            result = result * 10 + (c - '0');
        }else{
            return 0;
        }
    }
    return result * sign;
}

double stringToDouble(String s){
    double result = 0;
    double decimal = 0;
    bool decimalFlag = false;
    int sign = 0;
    bool skipFirst = false;
    if(s.text[0] == '-'){
        sign = -1;
        skipFirst = true;
    }else{
        sign = 1;
    }

    for(size_t i = 0; i < s.length; i++){
        if(skipFirst){
            i++;
            skipFirst = false;
        }
        char c = s.text[i];
        if(charIsDigit(c)){
            if(!decimalFlag){
                result = result * 10 + (c - '0');
            }else{
                decimal = decimal * 10 + (c - '0');
            }
        }else if(c == '.'){
            decimalFlag = true;
        }else {
            return 0;
        }
    }
    while(decimal >= 1){
        decimal /= 10;
    }
    result = (result + decimal) * sign; 
    return result;
}

//--------------------------------------------------- Expression Parsing library --------------------------------------------------------------

enum TokenType{
    TOKEN_NUMBER,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSED_BRACKET,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EOF
};

struct Token{
    TokenType type;
    union{
        int number;
        String character;
    };
};

struct Lexer{
    String str;
    uint32_t pos;
};


Token generateNextToken(Lexer * lexer){
    Token token = {};
    while(lexer->str.text[lexer->pos] == ' '){
        lexer->pos++;
    }
    char c = lexer->str.text[lexer->pos];

    char value[64];
    int i = 0;
    while(charIsDigit(c)){
        value[i] = c;
        i++;
        lexer->pos++;
        c = lexer->str.text[lexer->pos];
    }
    value[i] = '\0';

    if(i > 0){
        token.type = TOKEN_NUMBER;
        token.number = atoi(stringCArr(value).text);
        return token;
    }

    switch(c){
        case '(':  token.type = TOKEN_OPEN_BRACKET; token.character = string("(");      break;
        case ')':  token.type = TOKEN_CLOSED_BRACKET; token.character = string(")");    break;
        case '+':  token.type = TOKEN_PLUS; token.character = string("+");              break;
        case '-':  token.type = TOKEN_MINUS; token.character = string("-");             break;
        case '/':  token.type = TOKEN_SLASH; token.character = string("/");             break;
        case '*':  token.type = TOKEN_STAR; token.character = string("*");              break;
        case '\0': token.type = TOKEN_EOF; token.character = string("EOF");             break;
    }
    lexer->pos++;
    return token;
}

enum ASTNodeType{
    ASTNODE_NUMBER,
    ASTNODE_OPERATION
};

struct ASTNode{
    ASTNodeType type;
    union{
        float number;
        struct{
            ASTNode *left;
            ASTNode *right;
            TokenType op;
        }operation;
    };
};

Token token;

ASTNode* parseNumber(Arena* arena, Lexer* l){
    ASTNode* node = arenaAllocStruct(arena, ASTNode);
    while(token.type == TOKEN_NUMBER){
        node->type = ASTNODE_NUMBER;
        node->number = token.number;
        token = generateNextToken(l);
    }
    return node;
}

ASTNode* parseFactor(Arena* arena, Lexer* l){
    ASTNode* left = parseNumber(arena, l);

    while(token.type == TOKEN_STAR || token.type == TOKEN_SLASH){
        ASTNode* node = arenaAllocStruct(arena, ASTNode);
        node->type = ASTNODE_OPERATION;
        node->operation.op = token.type;
        node->operation.left = left;
        token = generateNextToken(l);
        ASTNode* right = parseNumber(arena, l);
        node->operation.right = right;
        left = node;
    }
    return left;
}


ASTNode* parseExpression(Arena* arena, Lexer* l){
    ASTNode* left = parseFactor(arena, l);

    while(token.type == TOKEN_PLUS || token.type == TOKEN_MINUS){
        ASTNode* node = arenaAllocStruct(arena, ASTNode);
        node->type = ASTNODE_OPERATION;
        node->operation.op = token.type;
        node->operation.left = left;
        token = generateNextToken(l);
        ASTNode* right = parseFactor(arena, l);
        node->operation.right = right;
        left = node;
    }
    return left;
};

ASTNode* parseLine(Arena* arena, String line){
    Lexer l;
    l.str = line;
    l.pos = 0;
    ASTNode* root = arenaAllocStruct(arena, ASTNode);
    token = generateNextToken(&l);
    root = parseExpression(arena, &l);
    return root;
}

double evaluate(ASTNode* root){
    if(root->type == ASTNODE_NUMBER){
        return root->number;
    }else if(root->type == ASTNODE_OPERATION){
        double l = evaluate(root->operation.left);
        double r = evaluate(root->operation.right);
        if(root->operation.op == TOKEN_PLUS){
            return l + r;
        }else if(root->operation.op == TOKEN_MINUS){
            return l - r;
        }else if(root->operation.op == TOKEN_STAR){
            return l * r;
        }else if(root->operation.op == TOKEN_SLASH){
            return l / r;
        }
    }
    return 0;
}


//--------------------------------------------------- MAIN --------------------------------------------------------------


int main(void){
    Arena* arena = initArena(1 * GB); //1 GB
    char* chunk = (char*)arenaAlloc(arena, 64);
    snprintf(chunk, 64, "ciao mondo");
    printf("%s\n",chunk);
    String s = string("ciao"); //Static allocated string
    s = string ("aaa");
    printf("%s, %d\n",s.text, s.length);
    String s2 = pushString(arena, "ciao mondo"); //Dynamic allocated string
    printf("%s, %d\n",s2.text, s2.length);

    String numS = string("123123.1231");
    double f2 = stringToDouble(numS);
    printf("%f\n", f2);

    //expression parsing testing
    String s1 = string("2 - 1 * 2 + 10");
    ASTNode* root = parseLine(arena, s1);
    printf("%f\n", evaluate(root));
    while(1){
        String expression;
        expression.text = arenaAllocArray(arena, char, 100);
        expression.length = 100;
        printf(">> ");
        scanf_s("%99s",expression.text);
        ASTNode* root = parseLine(arena, expression);
        printf("%f\n", evaluate(root));
        clearArena(arena);
    }
    return 0;
}