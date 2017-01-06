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
JSON(JavaScript Object Notation) ��һ�������������ݽ�����ʽ��
��������ݽ������ԡ� �������Ķ��ͱ�д��ͬʱҲ���ڻ�������������(һ�������������紫������)��
JSON �﷨����
JSON �﷨�� JavaScript �����ʾ�﷨���Ӽ���
�����ڼ�ֵ����
�����ɶ��ŷָ�
�����ű������
�����ű�������
JSON ����/ֵ��
JSON ���ݵ���д��ʽ�ǣ�����/ֵ�ԡ�
����/ֵ������е�����д��ǰ�棨��˫�����У���ֵ��д�ں���(ͬ����˫������)���м���ð�Ÿ�����
1
"firstName":"John"
����������⣬�ȼ������� JavaScript ��䣺
1
firstName="John"
JSON ֵ
JSON ֵ�����ǣ�
���֣������򸡵�����
�ַ�������˫�����У�
�߼�ֵ��true �� false��
���飨�ڷ������У�
�����ڻ������У�
null

���� / ֵ��
������򵥵���ʽ������������������ JSON ��ʾ"���� / ֵ��"��
1
{"firstName":"Brett"}
���ʾ���ǳ�����������ʵ���ϱȵ�Ч�Ĵ��ı�"���� / ֵ��"ռ�ø���Ŀռ䣺
1
firstName=Brett
���ǣ��������"���� / ֵ��"����һ��ʱ��JSON �ͻ����ֳ����ļ�ֵ�ˡ����ȣ����Դ����������"���� / ֵ��"�� ��¼�����磺
1
{"firstName":"Brett","lastName":"McLaughlin","email":"aaaa"}
���﷨��������������"���� / ֵ��"��Ȳ�û�кܴ�����ƣ���������������� JSON ������ʹ�ã����ҿɶ��Ը��á����磬����
ȷ�ر�ʾ��������ֵ����ͬһ��¼��һ���֣�������ʹ��Щֵ����ĳ����ϵ��

�˴�ʵ�ʲ�������:
�����ڸ��Եĳ����н���һ����������,��ʾ���Խ��յ�����.��ʵ���ǽ�����,���ÿ��׼������һ����ָ��ǰ����Ҫ��λ��.
Ȼ���������ַ������н���,�ѻ�ȡ��������ֵ��д����Ӧ�����ƺ���.
���ѽ�����.����������ֱ��������������,�����ڽ������в��ҵ�.
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atoi.h"
#include "json.h"

#define MAXPARSERS           2

#define MAXSTATES            10			//-���ܱ�ʾһ�ν�����������,����״̬���Ϊ10
#define MAXSTACK             5
#define MAXERRORBUFFER       20

#define MAXNAME              30
#define MAXVALUE             160   // was 80

#define STATE_NONE           0
#define STATE_INOBJECT       1			//-׼������һ��Ŀ��֮ǰ����Ҫ����һ���µĶ�ջ��,׼����¼�µĶ�����Ϣ
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

static int isWhiteSpace(char c) {//-�ǿյľͷ���1
    return ( (c == ' ') || (c == '\r') || (c == '\n') || (c == '\t') );
}

static int isSign(char c) {//-��-��+�ͷ���1
    return ( (c == '-') || (c == '+') );
}

static int isNum(char c) {//-�����ַ���1
    return ( (c >= '0') && (c <= '9') );
}

static int isAlpha(char c) {//-��26����ĸ֮һ,����1
    return ( ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) );
}

static int isValidNameChar(char c) {//-�ж��Ƿ�����Ч�����ַ�,�Ƿ���1
    return ( isAlpha(c) || isNum(c) || (c == '_' ) || (c == '+') );
}

static void appendName(char c) {//-������������һ���ַ�
    int len = strlen(parser->name[parser->stack]);
    if (len < MAXNAME) {
        parser->name[parser->stack][len] = c;
        parser->name[parser->stack][len + 1] = '\0';
    } else {
        // printf( "Name too long\n" );
        jsonError(ERR_NAMETOOLONG);
    }
}

static void appendValue(char c) {//-����һ����Чֵ�ַ�
    int len = strlen(parser->value[parser->stack]);
    if (len < MAXVALUE) {
        parser->value[parser->stack][len] = c;
        parser->value[parser->stack][len + 1] = '\0';
    } else {
        // printf( "Value too long\n" );
        jsonError(ERR_VALUETOOLONG);
    }
}

static void setState(int st) {//-��׼��ĳЩ״��֮ǰ���б�Ҫ��ʼ��
    // printf("Set state %d\n", st);
    parser->state = st;	//-��������״̬

    switch (parser->state) {//-�������µ�״̬,�������ַ�֮ǰ���б�Ҫ�ĳ�ʼ��
        case STATE_INNAME:
            // printf( "Reset name\n" );
            parser->name[parser->stack][0] = '\0';
            break;
        case STATE_TOVALUE:
            // printf( "Reset value\n" );
            parser->value[parser->stack][0] = '\0';	//-�ڽ�������֮ǰ��ʼ���ռ�
            break;
        case STATE_INOBJECT:
        case STATE_INARRAY:
            // printf("Push name '%s'\n", parser->name[parser->stack]);
            if (++parser->stack < MAXSTACK) {//-��¼��Ϣʹ���˶�ջ�ĸ���,������Ǳ�����һ��,Ϊ��¼����׼��
                parser->name[parser->stack][0] = '\0';
            } else {
                jsonError(ERR_INTERNAL);
            }
            break;
    }
}

static void toState(int st) {//-�������ѹջ����
    // printf("To state %d\n", st);
    parser->states[parser->stateIndex] = parser->state;	//-�ȼ�¼��ǰ��״̬,Ȼ���л�״̬
    if (++parser->stateIndex < MAXSTATES) {
        setState(st);	//-����������״̬
    } else {
        jsonError(ERR_INTERNAL);
    }
}

static void upState(void) {//-ǰ���н�ջ,�����ǳ�ջ,��ջӦ�����Ѿ�����������������Ϣ,���ڿ���ִ��������
    // printf("Up state\n");
    if (parser->stateIndex > 0) {
        parser->state = parser->states[--(parser->stateIndex)];	//-ȡǰһ��״̬
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

static void jsonEatNoLog(char c) {//-������������Ľ���������,Ȼ����ݲ�ͬ�Ķ�����ò�ͬ�Ĵ�������,���������������Ľ��պ�Ӧ��
    int again = 0;	//-�������ַ��������,�ڽ���һ��

    // int prevstate = parser->state;

    switch (parser->state) {//-��¼��������ǰ��״̬
        case STATE_NONE:
            if (c == '{') {//-��������ַ��ͽ�������Ĵ���,һ�����̾�ʵ����JSON����
                if (parser->onObjectStart != NULL) parser->onObjectStart(parser->name[parser->stack]);	//-�Կ��ܵĽ�����ֵ����ֵ
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
                setState(STATE_INNAME);	//-˵�����������������
            } else if (!isWhiteSpace(c)) {
                jsonError(ERR_PARSE_NAME);
            }
            break;

        case STATE_INNAME:
            if (c == '"') {//-�����������¼,������Ǵ����﷨
                setState(STATE_TOCOLUMN);	//-�л�������һ���ȴ���״̬
            } else if (isValidNameChar(c)) {
                appendName(c);	//-��ȷ����ջ���¼������������Ϣ
            } else {
                jsonError(ERR_PARSE_ILLNAME);
            }
            break;

        case STATE_TOCOLUMN:	//-�ڴ�����:�ַ�
            if (c == ':') {
                setState(STATE_TOVALUE);	//-˵��������յ����ַ�������ֵ�����
            } else if (!isWhiteSpace(c)) {	//-�������Ĳ��ǿ��ַ�,���Ǵ����ʽ,û�б�Ҫ����������
                jsonError(ERR_PARSE_ASSIGNMENT);
            }
            break;

        case STATE_TOVALUE:
            if (c == '"') {
                parser->isSlash = 0;
                setState(STATE_INSTRING);
            } else if (isNum(c) || isSign(c)) {//-�ж��Ƿ�������,��ʹ��Ҳ���ַ���ʽ��,����������
                appendValue(c);	//-�ڶ�ջ������һ������
                setState(STATE_INNUM);
            } else if (c == '[') {
                if (parser->onArrayStart != NULL) parser->onArrayStart(parser->name[parser->stack]);
                setState(STATE_INARRAY);
                toState(STATE_TONAME);
            } else if (c == '{') {//-˵����ֵ���������Ŀ����Ŀ��
                if (parser->onObjectStart != NULL) parser->onObjectStart(parser->name[parser->stack]);	//-��һ����Ŀ�꿪ʼ֮ǰ,�ѵ�ǰ��ջ����������
                setState(STATE_INOBJECT);
                toState(STATE_TONAME);
            } else if (!isWhiteSpace(c)) {
                jsonError(ERR_PARSE_VALUE);
            }
            break;

        case STATE_INSTRING:
            if (!parser->isSlash && c == '\\') {	//-�ж�б��
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
                appendValue(c);	//-������Чֵ
            }
            break;

        case STATE_INNUM:
            if (isNum(c)) {
                appendValue(c);
            } else {
                // printf( "-------> onInteger( Name='%s', value='%s' )\n", parser->name[parser->stack], parser->value[parser->stack] );
                if (parser->onInteger != NULL) {
                     parser->onInteger( parser->name[parser->stack],
                                        Atoi( parser->value[parser->stack] ) );	//-���������������Чֵ,�Ͱѽ������������ݽ�����д
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
                    again = 1;	//-��ʾ�����ַ���,ֱ������һ�ν���
                }
            }
            break;
    }

    // printf( "Eat %c, state = %d -> %d\n", c, prevstate, parser->state );

//    printf("%c  ->  S[%d]=", c, parser->stateIndex);
//    int i;
//    for (i = 0; i < parser->stateIndex; i++) printf("%d ", parser->states[i]);
//    printf(", state=%d, A=%d - %d\n", parser->state, parser->allowComma, parser->again);

    if (again) jsonEatNoLog(c);	//-��һ���ַ���ʼ����,�����Ҫ�Ļ������Լ��ٶ�ȡ��һ������
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
    parser->state           = STATE_NONE;	//-��¼�˵�ǰ�ȴ���״̬
    parser->stateIndex      = 0;	//-���Ҳ�Ƕ�ջʹ�õ�,�����Ƕ�ջ��Чֵ��ƫ����,�����ѹջ״̬ƫ��,��������,��ʵ�����׵�
    parser->stack           = 0;	//-��ջ��¼����һ����Ҫ�����Ķ��������,�ȴ��������֮��ʼһ������ջ����
    parser->name[parser->stack][0]  = '\0';
    parser->value[parser->stack][0] = '\0';
    parser->errbufIndex     = 0;
    parser->numchars        = 0;
}

/**
 * \brief Parse a JSON stream, one character at a time
 * \param c Character to parse
 */
void jsonEat(char c) {//-��ʼ�����Ľ����

    parser->numchars++;

    // Log character in error buffer
    if (!isWhiteSpace(c)) {
        parser->errbuf[parser->errbufIndex++] = c;
        if (parser->errbufIndex >= MAXERRORBUFFER) parser->errbufIndex = 0;
    }

    jsonEatNoLog(c);
}
//-���涼�Ǹ�һ�������ṹ��ֵ��,�����Ͷ�̬�Ĵ�����һ��������
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

int jsonGetSelected( void ) {	//-��õ�ǰ�����ṹ�������е�λ��
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
        parser = &parserData[p-1];	//-��¼�˽����ṹ������
    } else {
        printf( "Error: parser underflow\n" );
    }
}
