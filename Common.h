#ifndef COMMON_H
#define COMMON_H
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <utility>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <cstdlib>
#include <cassert>
#include <functional>
using namespace std;


typedef enum //枚举类型，保存词素类型
{
	/* 结束标志和错误标志 */
	ENDFILE, ERROR,
	/* 保留字 */
	IF, ELSE, INT, RETURN, VOID, WHILE,
	/* 变量名和数字 */
	ID, NUM,
	/* 特殊字符 */
	/*  {       }    >=   <=   !=   ==    =    <   >    +      -     *     /     (        )     ;     ,   */
	LBRACE, RBRACE, GTE, LTE, NEQ, EQ, ASSIGN, LT, GT, PLUS, MINUS, MULT, DIV, LPAREN, RPAREN, SEMI, COMMA,
	/* 行注释  段注释 */
	LCOMMENT, PCOMMENT,
	/*换行符*/
	NEXTLINE
} LexicalType;

typedef pair<LexicalType, string> Token;

struct Quaternary {
	string op;//操作符
	string src1;//源操作数1
	string src2;//源操作数2
	string des;//目的操作数
};

struct Block {
	string name;//基本块的名称
	vector<Quaternary> codes;//基本块中的四元式
	int next1;//基本块的下一连接块
	int next2;//基本块的下一连接块
};

void outputError(string err);
#endif // !COMMON_H
