// ------------------------------------------------------------------
// JSON stream parser
// ------------------------------------------------------------------
// Parses a JSON steam one character at a time and uses callbacks
// to the calling processes
// Can handle simple JSON strings and is very suitable for
// embedded systems with small footprint
// ------------------------------------------------------------------
// Author:    nlv10677
// Copyright: NXP B.V. 2014. All rights reserved
// ------------------------------------------------------------------

/** \file
 * \brief JSON stream parser. Parses a JSON steam one character at a time and uses callbacks to
 * the calling processes
 */
 
/*
JSON(JavaScript Object Notation) 是一种轻量级的数据交换格式。
理想的数据交换语言。 易于人阅读和编写，同时也易于机器解析和生成(一般用于提升网络传输速率)。
JSON 语法规则
JSON 语法是 JavaScript 对象表示语法的子集。
数据在键值对中
数据由逗号分隔
花括号保存对象
方括号保存数组
JSON 名称/值对
JSON 数据的书写格式是：名称/值对。
名称/值对组合中的名称写在前面（在双引号中），值对写在后面(同样在双引号中)，中间用冒号隔开：
1
"firstName":"John"
这很容易理解，等价于这条 JavaScript 语句：
1
firstName="John"
JSON 值
JSON 值可以是：
数字（整数或浮点数）
字符串（在双引号中）
逻辑值（true 或 false）
数组（在方括号中）
对象（在花括号中）
null

名称 / 值对
按照最简单的形式，可以用下面这样的 JSON 表示"名称 / 值对"：
1
{"firstName":"Brett"}
这个示例非常基本，而且实际上比等效的纯文本"名称 / 值对"占用更多的空间：
1
firstName=Brett
但是，当将多个"名称 / 值对"串在一起时，JSON 就会体现出它的价值了。首先，可以创建包含多个"名称 / 值对"的 记录，比如：
1
{"firstName":"Brett","lastName":"McLaughlin","email":"aaaa"}
从语法方面来看，这与"名称 / 值对"相比并没有很大的优势，但是在这种情况下 JSON 更容易使用，而且可读性更好。例如，它明
确地表示以上三个值都是同一记录的一部分；花括号使这些值有了某种联系。

此处实际操作流程:
首先在各自的程序中建立一个名称数组,表示可以接收的命令.其实就是解析器,这个每次准备处理一条新指令前都需要复位下.
然后对输入的字符串进行解析,把获取的最新数值填写到对应的名称后面.
最后把结果输出.这个结果不是直接来自输入语句的,而是在解析器中查找的.
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atoi.h"
#include "json.h"

#define MAXPARSERS           2

#define MAXSTATES            10			//-可能表示一次解析的最大深度,连续状态最大为10
#define MAXSTACK             5
#define MAXERRORBUFFER       20

#define MAXNAME              30
#define MAXVALUE             160   // was 80

#define STATE_NONE           0
#define STATE_INOBJECT       1			//-准备接收一个目标之前都需要开辟一个新的堆栈层,准备记录新的对象信息
#define STATE_TONAME         2
#define STATE_INNAME         3
#define STATE_TOCOLUMN       4
#define STATE_TOVALUE        5
#define STATE_INSTRING       6
#define STATE_INNUM          7
#define STATE_INARRAY        8
#define STATE_OUTVALUE       9

#define ERR_NONE             0
#define ERR_DISCARD          1
#define ERR_NAMETOOLONG      2
#define ERR_VALUETOOLONG     3
#define ERR_PARSE_OBJECT     4
#define ERR_PARSE_NAME       5
#define ERR_PARSE_ILLNAME    6
#define ERR_PARSE_ASSIGNMENT 7
#define ERR_PARSE_VALUE      8
#define ERR_PARSE_ARRAY      9
#define ERR_INTERNAL         10

// #define GENERATE_ERROR_ON_DISCARD   1

static char * json_errors[] = {
    "none",
    "discard",
    "name too long",
    "value too long",
    "parsing object",
    "parsing name",
    "illegal name char",
    "parsing assignment",
    "parsing value",
    "parsing array",
    "internal error"
};

// -------------------------------------------------------------
// Globals
// -------------------------------------------------------------

typedef struct {
    int state;
    int states[MAXSTATES];
    int stateIndex;

    int allowComma;
    int isSlash;

    int stack;
    char name[MAXSTACK][MAXNAME];
    char value[MAXSTACK][MAXVALUE];

    char errbuf[MAXERRORBUFFER];
    int errbufIndex;
    int numchars;

    OnError          onError;
    OnObjectStart    onObjectStart;
    OnObjectComplete onObjectComplete;
    OnArrayStart     onArrayStart;
    OnArrayComplete  onArrayComplete;
    OnString         onString;
    OnInteger        onInteger;

} parser_data_t;

static parser_data_t parserData[MAXPARSERS];
static parser_data_t * parser = &parserData[0];

// -------------------------------------------------------------
// Error handling
// -------------------------------------------------------------

static void jsonError(int err) {
    char locerrbuf[MAXERRORBUFFER + 2];

    int i;
    int start;
    int len = parser->numchars;
    if (len >= MAXERRORBUFFER) len = MAXERRORBUFFER;
    start = parser->errbufIndex - len;
    if (start < 0) start += MAXERRORBUFFER;
    for (i = 0; i < len; i++) {
        locerrbuf[i] = parser->errbuf[start++];
        if (start >= MAXERRORBUFFER) start = 0;
    }
    locerrbuf[len] = '\0';

    // printf("JSON error %d (%s) @ %s\n", err, json_errors[err], locerrbuf);
    if (parser->onError != NULL) parser->onError(err, json_errors[err], locerrbuf);

    jsonReset();
}

// -------------------------------------------------------------
// Helpers
// -------------------------------------------------------------

static int isWhiteSpace(char c) {//-是空的就返回1
    return ( (c == ' ') || (c == '\r') || (c == '\n') || (c == '\t') );
}

static int isSign(char c) {//-是-或+就返回1
    return ( (c == '-') || (c == '+') );
}

static int isNum(char c) {//-是数字返回1
    return ( (c >= '0') && (c <= '9') );
}

static int isAlpha(char c) {//-是26个字母之一,返回1
    return ( ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) );
}

static int isValidNameChar(char c) {//-判断是否是有效名称字符,是返回1
    return ( isAlpha(c) || isNum(c) || (c == '_' ) || (c == '+') );
}

static void appendName(char c) {//-在名称中增加一个字符
    int len = strlen(parser->name[parser->stack]);
    if (len < MAXNAME) {
        parser->name[parser->stack][len] = c;
        parser->name[parser->stack][len + 1] = '\0';
    } else {
        // printf( "Name too long\n" );
        jsonError(ERR_NAMETOOLONG);
    }
}

static void appendValue(char c) {//-增加一个有效值字符
    int len = strlen(parser->value[parser->stack]);
    if (len < MAXVALUE) {
        parser->value[parser->stack][len] = c;
        parser->value[parser->stack][len + 1] = '\0';
    } else {
        // printf( "Value too long\n" );
        jsonError(ERR_VALUETOOLONG);
    }
}

static void setState(int st) {//-在准备某些状况之前进行必要初始化
    // printf("Set state %d\n", st);
    parser->state = st;	//-主动设置状态

    switch (parser->state) {//-根据最新的状态,在输入字符之前进行必要的初始化
        case STATE_INNAME:
            // printf( "Reset name\n" );
            parser->name[parser->stack][0] = '\0';
            break;
        case STATE_TOVALUE:
            // printf( "Reset value\n" );
            parser->value[parser->stack][0] = '\0';	//-在接收数据之前初始化空间
            break;
        case STATE_INOBJECT:
        case STATE_INARRAY:
            // printf("Push name '%s'\n", parser->name[parser->stack]);
            if (++parser->stack < MAXSTACK) {//-记录信息使用了堆栈的概念,这里就是便宜了一个,为记录名字准备
                parser->name[parser->stack][0] = '\0';
            } else {
                jsonError(ERR_INTERNAL);
            }
            break;
    }
}

static void toState(int st) {//-这个就是压栈函数
    // printf("To state %d\n", st);
    parser->states[parser->stateIndex] = parser->state;	//-先记录当前的状态,然后切换状态
    if (++parser->stateIndex < MAXSTATES) {
        setState(st);	//-再设置最新状态
    } else {
        jsonError(ERR_INTERNAL);
    }
}

static void upState(void) {//-前面有进栈,这里是出栈,出栈应该是已经解析出了完整的信息,现在可以执行命令了
    // printf("Up state\n");
    if (parser->stateIndex > 0) {
        parser->state = parser->states[--(parser->stateIndex)];	//-取前一个状态
        if (parser->state == STATE_INOBJECT || parser->state == STATE_INARRAY) {
            if (parser->stack > 0) {
                parser->stack--;
                // printf("Pop name '%s'\n", parser->name[parser->stack]);
                parser->allowComma = 1;
            } else {
                jsonError(ERR_INTERNAL);
            }
        } else if (parser->state == STATE_NONE) {
            // printf( "Reset name - 2\n" );
            parser->name[parser->stack][0] = '\0';
        }
    } else {
        jsonError(ERR_INTERNAL);
    }
}

// -------------------------------------------------------------
// Eat character
// -------------------------------------------------------------

static void jsonEatNoLog(char c) {//-这个里面完整的解析了数据,然后根据不同的定义调用不同的处理函数,这样就完成了命令的接收和应答
    int again = 0;	//-不读入字符的情况下,在进入一次

    // int prevstate = parser->state;

    switch (parser->state) {//-记录解析器当前的状态
        case STATE_NONE:
            if (c == '{') {//-遇到这个字符就进行下面的处理,一套流程就实现了JSON解析
                if (parser->onObjectStart != NULL) parser->onObjectStart(parser->name[parser->stack]);	//-对可能的解析数值赋初值
                toState(STATE_INOBJECT);
                toState(STATE_TONAME);
                parser->allowComma = 0;
            } else if (c == '"') {
                toState(STATE_INNAME);
            } else if (!isWhiteSpace(c)) {
#ifdef GENERATE_ERROR_ON_DISCARD
                jsonError(ERR_DISCARD);
#endif
            }
            break;

        case STATE_INOBJECT:
            if (c == '}') {
                // printf( "-------> onObjectComplete( '%s' )\n", parser->name[parser->stack] );
                if (parser->onObjectComplete != NULL) parser->onObjectComplete(parser->name[parser->stack]);
                upState();
            } else if (c == '"') {
                toState(STATE_INNAME);
            } else if (c == ',' && parser->allowComma) {
                parser->allowComma = 0;
                toState(STATE_TONAME);
            } else if (!isWhiteSpace(c)) {
                jsonError(ERR_PARSE_OBJECT);
            }
            break;

        case STATE_TONAME:
            if (c == '"') {
                setState(STATE_INNAME);	//-说明即将输入的是名称
            } else if (!isWhiteSpace(c)) {
                jsonError(ERR_PARSE_NAME);
            }
            break;

        case STATE_INNAME:
            if (c == '"') {//-遇到这结束记录,否则就是错误语法
                setState(STATE_TOCOLUMN);	//-切换到了下一个等待的状态
            } else if (isValidNameChar(c)) {
                appendName(c);	//-在确定的栈层记录完整的名字信息
            } else {
                jsonError(ERR_PARSE_ILLNAME);
            }
            break;

        case STATE_TOCOLUMN:	//-期待输入:字符
            if (c == ':') {
                setState(STATE_TOVALUE);	//-说明下面接收到的字符串是数值意义的
            } else if (!isWhiteSpace(c)) {	//-如果输入的不是空字符,就是错误格式,没有必要继续解析了
                jsonError(ERR_PARSE_ASSIGNMENT);
            }
            break;

        case STATE_TOVALUE:
            if (c == '"') {
                parser->isSlash = 0;
                setState(STATE_INSTRING);
            } else if (isNum(c) || isSign(c)) {//-判断是否是数字,即使是也是字符形式的,不是整型数
                appendValue(c);	//-在堆栈层增加一个数字
                setState(STATE_INNUM);
            } else if (c == '[') {
                if (parser->onArrayStart != NULL) parser->onArrayStart(parser->name[parser->stack]);
                setState(STATE_INARRAY);
                toState(STATE_TONAME);
            } else if (c == '{') {//-说明数值里面可能是目标套目标
                if (parser->onObjectStart != NULL) parser->onObjectStart(parser->name[parser->stack]);	//-在一个新目标开始之前,把当前堆栈层的名字输出
                setState(STATE_INOBJECT);
                toState(STATE_TONAME);
            } else if (!isWhiteSpace(c)) {
                jsonError(ERR_PARSE_VALUE);
            }
            break;

        case STATE_INSTRING:
            if (!parser->isSlash && c == '\\') {	//-判断斜线
                parser->isSlash = 1;
            } else if (parser->isSlash) {
                parser->isSlash = 0;
                appendValue('\\');
                appendValue(c);
            } else if (c == '"') {
                // printf( "-------> onString( '%s', '%s' )\n", parser->name[parser->stack], parser->value[parser->stack] );
                if (parser->onString != NULL) parser->onString(parser->name[parser->stack], parser->value[parser->stack]);
                setState(STATE_OUTVALUE);
            } else {
                appendValue(c);	//-增加有效值
            }
            break;

        case STATE_INNUM:
            if (isNum(c)) {
                appendValue(c);
            } else {
                // printf( "-------> onInteger( Name='%s', value='%s' )\n", parser->name[parser->stack], parser->value[parser->stack] );
                if (parser->onInteger != NULL) {
                     parser->onInteger( parser->name[parser->stack],
                                        Atoi( parser->value[parser->stack] ) );	//-如果定义了整形有效值,就把解析出来的数据进行填写
                }
                setState(STATE_OUTVALUE);
                again = 1;
            }
            break;

        case STATE_INARRAY:
            if (c == ']') {
                // printf( "-------> onArrayComplete( '%s' )\n", parser->name[parser->stack] );
                if (parser->onArrayComplete != NULL) parser->onArrayComplete(parser->name[parser->stack]);
                upState();
                // setState( STATE_OUTVALUE );
            } else if (c == '"') {
                toState(STATE_INNAME);
            } else if (c == ',' && parser->allowComma) {
                parser->allowComma = 0;
                toState(STATE_TONAME);
            } else if (!isWhiteSpace(c)) {
                jsonError(ERR_PARSE_ARRAY);
            }
            break;

        case STATE_OUTVALUE:
            if (!isWhiteSpace(c)) {
                if (c == ',') {
                    toState(STATE_TONAME);
                } else {
                    upState();
                    again = 1;	//-表示不读字符了,直接再来一次解析
                }
            }
            break;
    }

    // printf( "Eat %c, state = %d -> %d\n", c, prevstate, parser->state );

//    printf("%c  ->  S[%d]=", c, parser->stateIndex);
//    int i;
//    for (i = 0; i < parser->stateIndex; i++) printf("%d ", parser->states[i]);
//    printf(", state=%d, A=%d - %d\n", parser->state, parser->allowComma, parser->again);

    if (again) jsonEatNoLog(c);	//-从一个字符开始解析,如果需要的话可以自己再读取下一个继续
}

// -------------------------------------------------------------
// Public Interface
// -------------------------------------------------------------

/**
 * \brief Reset the JSON parser
 */
void jsonReset(void) {
    parser->allowComma      = 0;
    parser->isSlash         = 0;
    parser->state           = STATE_NONE;	//-记录了当前等待的状态
    parser->stateIndex      = 0;	//-这个也是堆栈使用的,下面是堆栈有效值得偏移量,这个是压栈状态偏移,防错处理,其实是配套的
    parser->stack           = 0;	//-堆栈记录的是一个需要处理的对象或数组,等待解析完成之后开始一个个出栈处理
    parser->name[parser->stack][0]  = '\0';
    parser->value[parser->stack][0] = '\0';
    parser->errbufIndex     = 0;
    parser->numchars        = 0;
}

/**
 * \brief Parse a JSON stream, one character at a time
 * \param c Character to parse
 */
void jsonEat(char c) {//-开始解析的进入点

    parser->numchars++;

    // Log character in error buffer
    if (!isWhiteSpace(c)) {
        parser->errbuf[parser->errbufIndex++] = c;
        if (parser->errbufIndex >= MAXERRORBUFFER) parser->errbufIndex = 0;
    }

    jsonEatNoLog(c);
}
//-下面都是给一个解析结构赋值的,这样就动态的创建了一个解析体
void jsonSetOnError(OnError oe) {
    parser->onError = oe;
}

void jsonSetOnObjectStart(OnObjectStart oos) {
    parser->onObjectStart = oos;
}

void jsonSetOnObjectComplete(OnObjectComplete oo) {
    parser->onObjectComplete = oo;
}

void jsonSetOnArrayStart(OnArrayStart oas) {
    parser->onArrayStart = oas;
}

void jsonSetOnArrayComplete(OnArrayComplete oa) {
    parser->onArrayComplete = oa;
}

void jsonSetOnString(OnString os) {
    parser->onString = os;
}

void jsonSetOnInteger(OnInteger oi) {
    parser->onInteger = oi;
}

// -------------------------------------------------------------
// Parser switching (for nested parsing)
// -------------------------------------------------------------

int jsonGetSelected( void ) {	//-获得当前解析结构在数组中的位置
    int i;
    for ( i=0; i<MAXPARSERS; i++ ) {
        if ( parser == &parserData[i] ) return( i );
    }
    return( 0 );
}

void jsonSelectNext( void ) {
    int p = jsonGetSelected();
    if ( p < ( MAXPARSERS - 1 ) ) {
        // printf( "JSON select %d\n", p+1 );
        parser = &parserData[p+1];
    } else {
        printf( "Error: parser overflow\n" );
    }
}

void jsonSelectPrev( void ) {
    int p = jsonGetSelected();
    if ( p > 0 ) {
        // printf( "JSON select %d\n", p-1 );
        parser = &parserData[p-1];	//-记录了解析结构的数据
    } else {
        printf( "Error: parser underflow\n" );
    }
}

