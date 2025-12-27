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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

// 缓冲区定义
static char buf[65536] = {};
static int buf_pos = 0;

static char code_buf[65536 + 128] = {};
static char *code_format =
    "#include <stdio.h>\n"
    "int main() { "
    "  unsigned result = %s; "
    "  printf(\"%%u\", result); "
    "  return 0; "
    "}";

static void gen(char *str)
{
  int len = strlen(str);
  if (buf_pos + len >= 65536)
    return; // 防止溢出
  strcpy(buf + buf_pos, str);
  buf_pos += len;
}

static void gen_num()
{
  char str[32];
  sprintf(str, "%d", rand() % 100);
  gen(str);
}

static void gen_rand_op()
{
  switch (rand() % 4)
  {
  case 0:
    gen("+");
    break;
  case 1:
    gen("-");
    break;
  case 2:
    gen("*");
    break;
  case 3:
    gen("/");
    break;
  }
}

static void gen_rand_expr()
{
  if (buf_pos > 1000)
  {
    gen_num();
    return;
  }

  switch (rand() % 3)
  {
  case 0:
    gen_num();
    break;
  case 1:
    gen("(");
    gen_rand_expr();
    gen(")");
    break;
  default:
    gen_rand_expr();
    gen_rand_op();
    gen_rand_expr();
    break;
  }
}

int main(int argc, char *argv[])
{
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1)
  {
    sscanf(argv[1], "%d", &loop);
  }

  int i;
  for (i = 0; i < loop; i++)
  {
    buf_pos = 0;
    buf[0] = '\0';

    gen_rand_expr();
    buf[buf_pos] = '\0';

    if (strlen(buf) == 0)
      continue;

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr 2>/dev/null");
    if (ret != 0)
      continue;
    fp = popen("/tmp/.expr 2>/dev/null", "r");

    assert(fp != NULL);

    int result;
    if (fscanf(fp, "%d", &result) == 1)
    {
      printf("%u %s\n", result, buf);
    }

    pclose(fp);
  }
  return 0;
}
