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
          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
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

  return true;
}

static bool check_parentheses(int p, int q)
{
  // If the expression does not start with '(' or end with ')', return false
  if (tokens[p].type != '(' || tokens[q].type != ')')
  {
    return false;
  }

  int balance = 0; // Counter for parenthesis balance

  // Iterate from p to q-1 (exclude the last parenthesis)
  for (int i = p; i < q; i++)
  {
    if (tokens[i].type == '(')
      balance++;
    if (tokens[i].type == ')')
      balance--;

    // If balance becomes 0, it means the first '(' is closed before the end
    // Example: (1 + 2) * (3 + 4) -> returns false
    if (balance == 0)
      return false;
  }

  // If loop finishes, check if parentheses are balanced
  return balance == 0;
}

static int find_main_operator(int p, int q)
{
  int op = -1;         // Position of the main operator
  int balance = 0;     // Parenthesis level
  int current_pri = 0; // Priority of current operator
  int best_pri = 0;    // Priority of the best operator found so far

  for (int i = p; i <= q; i++)
  {
    int type = tokens[i].type;

    // Handle parentheses: skip content inside them
    if (type == '(')
    {
      balance++;
      continue;
    }
    if (type == ')')
    {
      balance--;
      continue;
    }

    // Only look for operators outside of parentheses
    if (balance != 0)
      continue;

    // Skip numbers (they are not operators)
    if (type == TK_DEC || type == TK_HEX)
      continue;

    // Assign priority (smaller number = lower priority = main operator)
    if (type == TK_EQ)
      current_pri = 1; // Lowest priority
    else if (type == '+' || type == '-')
      current_pri = 4;
    else if (type == '*' || type == '/')
      current_pri = 5; // Highest priority
    else
      continue; // Not a supported operator

    // Update main operator if:
    // 1. No operator found yet (op == -1)
    // 2. Found an operator with lower or equal priority (right-associativity)
    if (op == -1 || current_pri <= best_pri)
    {
      op = i;
      best_pri = current_pri;
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
