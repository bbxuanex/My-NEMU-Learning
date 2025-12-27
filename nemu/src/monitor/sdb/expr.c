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
  TK_REG,

  /* TODO: Add more token types */

};

static struct rule
{
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE},          // spaces
    {"\\+", '+'},               // plus
    {"==", TK_EQ},              // equal
    {"-", '-'},                 // differ
    {"\\*", '*'},               // multiply
    {"/", '/'},                 // chuhao
    {"\\(", '('},               // left
    {"\\)", ')'},               // right
    {"0x[0-9a-fA-F]+", TK_HEX}, // Hexadecimal
    {"[0-9]+", TK_DEC},         // Decimal
    {"\\$[a-zA-Z0-9]+", TK_REG},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

void init_regex()
{
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++)
  {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0)
    {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token
{
  int type;
  char str[32];
} Token;

static Token tokens[65536] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e)
{
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0')
  {
    for (i = 0; i < NR_REGEX; i++)
    {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0)
      {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        switch (rules[i].token_type)
        {
        case TK_NOTYPE:
          // Do nothing for spaces, just skip
          break;
        case TK_REG:
          tokens[nr_token].type = TK_REG;
          if (substr_len < 32)
          {
            strncpy(tokens[nr_token].str, substr_start, substr_len);
            tokens[nr_token].str[substr_len] = '\0';
          }
          nr_token++;
          break;

        case TK_DEC:
        case TK_HEX:
          if (substr_len >= 32)
          {
            panic("buffer overflow");
          }
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
          break;
        default:
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';

          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
          break;
          if (nr_token >= 65536)
          {
            panic("Error: Token buffer overflow! Please increase the tokens array size!");
          }
          break;
        }
        break;
      }
    }

    if (i == NR_REGEX)
    {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
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
  int op = -1;
  int paren = 0;

  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == '(')
      paren++;
    else if (tokens[i].type == ')')
      paren--;

    if (paren == 0)
    {

      if (tokens[i].type == '+' || tokens[i].type == '-')
      {
        op = i;
      }
    }
  }

  if (op != -1)
    return op;
  paren = 0;
  for (int i = p; i <= q; i++)
  {
    if (tokens[i].type == '(')
      paren++;
    else if (tokens[i].type == ')')
      paren--;

    if (paren == 0)
    {
      if (tokens[i].type == '*' || tokens[i].type == '/')
      {
        op = i;
      }
    }
  }

  return op;
}

static word_t eval(int p, int q)
{
  printf("DEBUG eval: p=%d, q=%d\n", p, q);
  if (p > q)
  {
    // Bad expression
    return 0;
  }
  else if (p == q)
  {
    // Base case: single number
    // Convert string to unsigned long. 0 means auto-detect base (10 or 16)
    // 在 make_token() 匹配成功后

    if (tokens[p].type == TK_REG)
    {
      bool success;
      word_t val = isa_reg_str2val(tokens[p].str + 1, &success);
      if (!success)
      {
        printf("Unknown register: %s\n", tokens[p].str);
        return 0;
      }
      return val;
    }
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
      printf("[Error] Main operator not found in range [%d, %d]!\n", p, q);
      printf("Current expression substring: ");
      for (int k = p; k <= q; k++)
      {
        printf("%s", tokens[k].str);
      }
      printf("\n");
      return 0;
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
      if (val2 == 0)
      {
        printf("Error: Division by zero\n");
        return 0;
      }
      return (word_t)((int)val1 / (int)val2);
    case TK_EQ:
      return val1 == val2;
    default:
      assert(0);
    }
  }
  return 0;
}

word_t expr(char *e, bool *success)
{
  if (!make_token(e))
  {
    *success = false;
    return 0;
  }

  /* Start evaluation from the first to the last token */
  *success = true;
  return eval(0, nr_token - 1);
}
