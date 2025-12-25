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

#include <utils.h>
#include <stdio.h> // 引入这个头文件，不然 printf 会报错

NEMUState nemu_state = { .state = NEMU_STOP };

int is_exit_status_bad()
{
  int good = (nemu_state.state == NEMU_END && nemu_state.halt_ret == 0) ||
             (nemu_state.state == NEMU_QUIT);

  // 🔴 监控代码：打印出当前的状态值
  // state: 当前状态, QUIT: 退出状态的标准值, good: 判断结果
  printf("DEBUG: state=%d, QUIT=%d, good=%d\n", nemu_state.state, NEMU_QUIT, good);

  return !good;
}
