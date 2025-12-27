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
#include <common.h>
#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint
{
  int NO;
  // 记住用户的表达式字符串
  char expr[128];
  // 记住表达式上一次的值，和新值做比较
  word_t old_val;
  struct watchpoint *next;

  /* done for the first time at 2025.12.27 ——shuimushi*/
  /* TODO: add more sentences if necessary */

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;
#include <common.h>
WP *new_wp();
void free_wp(WP *wp);
bool scan_watchpoint();

void init_wp_pool()
{
  int i;
  for (i = 0; i < NR_WP; i++)
  {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}
WP *new_wp()
{
  // 检查是否还有空闲监视点
  Assert(free_ != NULL, "No free watchpoints");
  // Detach a node from the 'free_'list
  WP *wp = free_;
  free_ = free_->next;
  // Insert the node at the beginning of the 'head'list(active list)
  wp->next = head;
  head = wp;
  // Reset the node data to avoid dirty data
  wp->old_val = 0;
  memset(wp->expr, 0, sizeof(wp->expr));
  return wp;
}
void free_wp(WP *wp)
{
  // Basic safety check
  if (wp == NULL || head == NULL)
    return;
  // Search and remove 'wp'from the head list
  if (head == wp)
  {
    // Case A:the node to remove is the head
    head = head->next;
  }
  else
  {
    // Case B:The node is in the middle or end
    WP *prev = head;
    while (prev->next != NULL && prev->next != wp)
    {
      prev = prev->next;
    }
    // Ensure the node actually exists in the active list
    Assert(prev->next == wp, "The watchpoint is not found in the active list!");
    // Remove 'wp' from the link
    prev->next = wp->next;
  }
  // Return the node to 'free_'list
  wp->next = free_;
  free_ = wp;
}
bool scan_watchpoint()
{
  WP *p = head;
  bool found_change = false;
  while (p)
  {
    bool success;
    // Evaluate the expression
    word_t new_val = expr(p->expr, &success);
    if (!success)
    {
      // Note:If success is false,you might want to handle the error or ignore it.
      printf("Warning: Watchpoint %d expression evaluation failed!\n", p->NO);
      return false;
    }
    // Compare the new value with the old value
    if (new_val != p->old_val)
    {
      printf("Watchpoint %d: %s\n", p->NO, p->expr);
      printf("Old value = %u (0x%x)\n", p->old_val, p->old_val);
      printf("New value = %u (0x%x)\n", new_val, new_val);
      // Update the old value for the next comparison
      p->old_val = new_val;
      found_change = true;
    }
    p = p->next;
  }
  return found_change;
}
/* initial implement by [shuimushi] on 2025.12.27 */
/* TODO: Implement the functionality of watchpoint */
