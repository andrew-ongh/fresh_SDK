/**
 ****************************************************************************************
 *
 * @file menu.h
 *
 * @brief Declarations for application menu
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#ifndef MENUDEF_H_
#define MENUDEF_H_

#include <stdbool.h>

/** begin of menu definition - 'root' has to be always defined! */
#define MENU_BEGIN(name) const struct menu_item __MENU_ ## name [] = {
/** define menu item with submenu */
#define MENU_ITEM_SUBM(m_name, m_submenu) { .name = m_name, .submenu = __MENU_ ## m_submenu },
/** define menu item with handler function */
#define MENU_ITEM_VOID(m_name, m_func) { .name = m_name, .func = menu_ ## m_func ## _func },
/** define menu item with handler function and custom parameter */
#define MENU_ITEM_PARM(m_name, m_func, m_param) { \
                                                .name = m_name, \
                                                .func = menu_ ## m_func ## _func, \
                                                .param = (void *) m_param },
/** define menu item with handler function, can be 'checked' */
#define MENU_ITEM_CHKV(m_name, m_func) { \
                                                .name = m_name, \
                                                .func = menu_ ## m_func ## _func, \
                                                .checked_cb = menu_ ## m_func ## _checked_cb_func },
/** define menu item with handler function and custom parameter, can be 'checked' */
#define MENU_ITEM_CHKP(m_name, m_func, m_param) { \
                                                .name = m_name, \
                                                .func = menu_ ## m_func ## _func, \
                                                .param = (void *) m_param, \
                                                .checked_cb = menu_ ## m_func ## _checked_cb_func },
/** end of menu definition */
#define MENU_END() { /* end */ } };

struct menu_item;

/**
 * \brief Menu item handler function
 */
typedef void (* menu_func) (const struct menu_item *menu, bool checked);

/**
 * \brief Menu item 'checked' state callback function
 */
typedef bool (* checked_cb_func) (const struct menu_item *menu);

/**
 * \brief Menu item description
 */
struct menu_item {
        const char *name;
        const struct menu_item *submenu;
        const menu_func func;
        const void *param;
        const checked_cb_func checked_cb;
};

/**
 * \brief Draw application menu
 *
 */
void app_menu_draw(void);

/**
 * \brief Parse menu selection from string
 *
 * \param [in] s input string
 */
void app_menu_parse_selection(char *s);

#endif /* MENUDEF_H_ */
