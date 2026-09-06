/**
 * @file      shell_cmd_hyphen.h
 *
 * @brief     Allow Zephyr shell command name with hyphen.
 *
 * @author    Qorvo Paris
 *
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#ifndef ZEPHYR_APP_SRC_SHELL_CMD_HYPHEN_H_
#define ZEPHYR_APP_SRC_SHELL_CMD_HYPHEN_H_

#include <zephyr/shell/shell.h>

#undef SHELL_EXPR_CMD_ARG
#undef SHELL_CMD_ARG
#undef SHELL_CMD_REGISTER
#undef SHELL_CMD_ARG_REGISTER

#define SHELL_EXPR_CMD_ARG(_expr, _syntax, _syntax1, _subcmd, _help, _handler, \
			   _mand, _opt)                                        \
	{                                                                      \
		.syntax = (_expr) ? (const char *)_syntax1 : "",               \
		.help = (_expr) ? (const char *)_help : NULL,                  \
		.subcmd =                                                      \
		    (const union shell_cmd_entry *)((_expr) ? _subcmd : NULL), \
		.handler = (shell_cmd_handler)((_expr) ? _handler : NULL),     \
		.args = {                                                      \
			.mandatory = _mand,                                    \
			.optional = _opt                                       \
		}                                                              \
	}

#define SHELL_CMD_ARG(syntax, syntax1, subcmd, help, handler, mand, opt) \
	SHELL_EXPR_CMD_ARG(1, syntax, syntax1, subcmd, help, handler, mand, opt)

#define SHELL_CMD_ARG_REGISTER(syntax, syntax1, subcmd, help, handler,       \
			       mandatory, optional)                          \
	static const struct shell_static_entry UTIL_CAT(_shell_, syntax) =   \
	    SHELL_CMD_ARG(syntax, syntax1, subcmd, help, handler, mandatory, \
			  optional);                                         \
	static const TYPE_SECTION_ITERABLE(union shell_cmd_entry,            \
					   UTIL_CAT(shell_cmd_, syntax),     \
					   shell_root_cmds,                  \
					   UTIL_CAT(shell_cmd_, syntax)) = { \
	    .entry = &UTIL_CAT(_shell_, syntax)}
#define SHELL_CMD_REGISTER(syntax, syntax1, subcmd, help, handler) \
	SHELL_CMD_ARG_REGISTER(syntax, syntax1, subcmd, help, handler, 0, 0)

#endif /* ZEPHYR_APP_SRC_SHELL_CMD_HYPHEN_H_ */
