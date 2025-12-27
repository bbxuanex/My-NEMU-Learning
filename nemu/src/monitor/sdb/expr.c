/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum
{
  TK_NOTYPE = 256,
  TK_EQ,
  TK_DEC,
  TK_HEX,

  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE}, // spaces
    {"\\+", '+'},      // plus
    {"==", TK_EQ},     // equal
    {"-", '-'},        // differ
    {"\\*", '*'},      // multiply
    {"/", '/'},        // chuhao
    {"\\(", '('},      // left
    {"\\)", ')'},      // right
    {"0x[0-9a-fA-F]+", TK_HEX},//Hexadecimal
    {"[0-9]+", TK_DEC}, // Decimal
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[65536] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;
  printf("【调试】我是新程序！数组大小是 65536！nr_token地址: %p\n", &nr_token);

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        

        switch (rules[i].token_type) {
        case TK_NOTYPE:
          // Do nothing for spaces, just skip
          break;

        case TK_DEC:
        case TK_HEX:
          if (substr_len >= 32)
          {
            panic("buffer overflow");
          }
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          tokens[nr_token].type = rules[i].token_type; // 设置类型
          nr_token++;                                  // 记得这里也要加
          break;                                       // ❌ 加上 break，不要穿透到 default
        default:
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';

          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
          break;
          if (nr_token >= 65536)
          {
            panic("Token 太多啦！请把 tokens 数组改得更大！");
          }
          break;
        }
        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  printf("【调试】make_token 成功结束！共识别 %d 个 Token\n", nr_token);
  return true;
}

static bool check_parentheses(int p, int q)
{
  if (tokens[p].type != '(' || tokens[q].type != ')')
  {
    return false;
  }

  int n = 0;
  for (int i = p; i < q; i++)
  { 
    if (tokens[i].type == '(')
      n++;
    if (tokens[i].type == ')')
      n--;

    if (n == 0)
      return false;
  }

 
  if (tokens[q].type == ')')
    n--;

  return n == 0;
}

int find_main_operator(int p, int q)
{
  int op = -1;      // 记录主运算符的位置
  int paren = 0;    // 括号计数
  int min_prec = 0; // 当前记录的最低优先级（数值越大，优先级越高，我们想要最低的）
                    // 设定：+ - 优先级为 1， * / 优先级为 2

  for (int i = p; i <= q; i++)
  {
    // 1. 跳过括号内的内容
    if (tokens[i].type == '(')
    {
      paren++;
      continue;
    }
    if (tokens[i].type == ')')
    {
      paren--;
      continue;
    }
    if (paren > 0)
      continue; // 如果在括号里，直接跳过

    // 2. 判断当前 Token 是不是运算符，并获取它的优先级
    int curr_prec = 0;

    // 注意：这里要根据你自己的 enum 定义来修改
    // 如果你的 type 就是字符本身（比如 '+'），就这样写：
    if (tokens[i].type == '+' || tokens[i].type == '-')
      curr_prec = 1;
    else if (tokens[i].type == '*' || tokens[i].type == '/')
      curr_prec = 2;

    // 如果你用的是 TK_PLUS 这种 enum，请改成对应的 case
    // else if (tokens[i].type == TK_EQ) ... (这是后续 PA 可能会有的)

    else
      continue; // 不是运算符（是数字），跳过

    // 3. 核心逻辑：寻找“优先级最低”且“最右边”的运算符
    // 这里的 <= 是关键！
    // 当优先级相同时，我们更新 op，这样就能取到更靠右的那个
    // 例如 1 + 2 + 3，遇到第二个 + 时，优先级一样，更新 op，主运算符变成第二个 +
    // 这样分割成 (1+2) + 3，先算左边，符合左结合律

    if (op == -1 || curr_prec <= min_prec)
    {
      op = i;
      min_prec = curr_prec;
    }
  }

  return op;
}

static word_t eval(int p, int q)
{
  if (p > q)
  {
    // Bad expression
    return 0;
  }
  else if (p == q)
  {
    // Base case: single number
    // Convert string to unsigned long. 0 means auto-detect base (10 or 16)
    return strtoul(tokens[p].str, NULL, 0);
  }
  else if (check_parentheses(p, q) == true)
  {
    // The expression is surrounded by parentheses, remove them
    return eval(p + 1, q - 1);
  }
  else
  {
    // General case: split by main operator
    int op = find_main_operator(p, q);
    if (op == -1)
    {
      printf("【错误】无法在 [%d, %d] 范围内找到主运算符！\n", p, q);
      // 打印出这段有问题的表达式，方便调试
      printf("当前处理的表达式片段: ");
      for (int k = p; k <= q; k++)
      {
        printf("%s", tokens[k].str);
      }
      printf("\n");
      return 0; // 或者 assert(0);
    }
    // Recursively evaluate both sides
    word_t val1 = eval(p, op - 1);
    word_t val2 = eval(op + 1, q);

    // Perform calculation based on the operator type
    switch (tokens[op].type)
    {
    case '+':
      return val1 + val2;
    case '-':
      return val1 - val2;
    case '*':
      return val1 * val2;
    case '/':
      // Avoid division by zero
      return (val2 == 0 ? 0 : val1 / val2);
    case TK_EQ:
      return val1 == val2;
    default:
      assert(0); // Should not reach here
    }
  }
  return 0;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* Start evaluation from the first to the last token */
  *success = true;
  return eval(0, nr_token - 1);
}
