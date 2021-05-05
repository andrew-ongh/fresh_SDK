/**
 ****************************************************************************************
 *
 * @file menu.c
 *
 * @brief Implementation of application menu
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "menu.h"

/* 'root' menu has to be always defined */
extern const struct menu_item __MENU_root[];

#define MAX_MENU_DEPTH (3)

/** stack for nesting menu */
static const struct menu_item *menu_stack[MAX_MENU_DEPTH] = { __MENU_root };

/** current nesting level */
static size_t menu_depth = 0;

/*
 * bitmask with 'checked' state of currently displayed menu items, i.e. result of 'checked_cb'
 * callback for each menu item; cached 'checked' value is passed to handler when menu item is
 * selected, but handler can reevaluate this by calling 'checked_cb' callback manually, if this is
 * required for some reason
 */
static uint32_t menu_checked = 0;

/** try to handle menu option for given index */
static void do_numeric_opt(int opt)
{
        const struct menu_item *m = menu_stack[menu_depth];
        bool checked;

        /* on-screen we have 1..N, but internally we use 0..(N-1) so any negative number is invalid */
        if (--opt < 0) {
                return;
        }

        /* retrieve 'checked' value from cache */
        checked = menu_checked & (1 << opt);

        /* find option - can't just use index directly since we don't know how many entries are theres */
        while (opt && m->name) {
                m++;
                opt--;
        }

        if (!m->name || (!m->submenu && !m->func)) {
                return;
        }

        /* if there's no submenu, execute assosiated function */
        if (!m->submenu) {
                m->func(m, checked);
                return;
        }

        /* can't go deeper if already at max nesting depth */
        if (menu_depth >= MAX_MENU_DEPTH - 1) {
                return;
        }

        /* put submenu on stack */
        menu_depth++;
        menu_stack[menu_depth] = m->submenu;
}

void app_menu_draw(void)
{
        const struct menu_item *m = menu_stack[menu_depth];
        int pos = 0;

        /* reset 'checked' cache */
        menu_checked = 0;

        printf(NEWLINE ",=== select option and press [Enter]:" NEWLINE);

        while (m[pos].name) {
                char *sel_mark = "    ";

                if (m[pos].checked_cb) {
                        bool sel = m[pos].checked_cb(&m[pos]);
                        sel_mark =  sel ? "[*] " : "[ ] ";
                        /*
                         * Cache 'checked' value for menu, this will be includes in menu handler
                         * so there's no need to query again (but app still can choose to do this
                         * if necessary).
                         */
                        if (sel) {
                                menu_checked |= (1 << pos);
                        }
                }

                printf("| %2d - %s%s" NEWLINE, pos + 1, sel_mark, m[pos].name);
                pos++;
        }
        if (menu_depth) {
                printf("|  x - go up" NEWLINE);
        }

        printf("`==> ");

        fflush(stdout);
}

void app_menu_parse_selection(char *s)
{
        int opt;
        char *s_end;

        /* check if numeric option is selected */
        opt = strtol(s, &s_end, 10);
        if (s != s_end) {
                do_numeric_opt(opt);
                return;
        }

        /* skip leading whitespaces */
        while (*s && isspace((int) *s)) {
                s++;
        }

        /* go to end of word and null-terminate string */
        s_end = s;
        while (*s_end && !isspace((int) *s_end)) {
                s_end++;
        }
        *s_end = '\0';

        /* check if special option is selected (only one character allowed) */
        if (strlen(s) != 1) {
                return;
        }

        switch (tolower((int) s[0])) {
        case 'x':
                if (menu_depth > 0) {
                        menu_depth--;
                }
                break;
        }
}
