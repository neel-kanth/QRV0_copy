/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#ifndef CHERRY_SESSION_MANAGER_C
#define CHERRY_SESSION_MANAGER_C

#include "cherry_session_manager.h"

#include "cherry_log.h"

void cherry_register_session(struct cherry *ctx,
			     struct cherry_session *new_session)
{
	qmutex_lock(ctx->sessions_mutex, QOSAL_WAIT_FOREVER);
	new_session->next_session = ctx->sessions;
	ctx->sessions = new_session;
	qmutex_unlock(ctx->sessions_mutex);
}

bool cherry_session_is_registered(struct cherry *ctx,
				  struct cherry_session *session)
{
	struct cherry_session *session_current;

	qmutex_lock(ctx->sessions_mutex, QOSAL_WAIT_FOREVER);
	session_current = ctx->sessions;
	while (session_current) {
		if (session == session_current) {
			qmutex_unlock(ctx->sessions_mutex);
			return true;
		}
		session_current = session_current->next_session;
	}
	qmutex_unlock(ctx->sessions_mutex);
	return false;
}

void cherry_unregister_session(struct cherry *ctx,
			       struct cherry_session *session)
{
	qmutex_lock(ctx->sessions_mutex, QOSAL_WAIT_FOREVER);
	if (ctx->sessions == session) {
		ctx->sessions = session->next_session;
	} else {
		struct cherry_session *tmp_session = ctx->sessions;

		while (tmp_session->next_session) {
			if (tmp_session->next_session == session) {
				tmp_session->next_session =
					session->next_session;
				qmutex_unlock(ctx->sessions_mutex);
				return;
			}
			tmp_session = tmp_session->next_session;
		}
		QLOGE("%s: session %p not found on session list", __func__,
		      session);
	}
	qmutex_unlock(ctx->sessions_mutex);
}

void cherry_force_stop_session(struct cherry_session *session_base)
{
	struct cherry *ctx = session_base->cherry_ctx;

	cherry_unregister_session(ctx, session_base);

	/* Indicate session has been forced stop. */
	cherry_session_force_stop(session_base);
	/* Remove the pending tasks on the session. */
	cherry_thread_remove_pending_tasks(&ctx->thread_ctx, session_base);
}

void cherry_force_stop_all_sessions(struct cherry *ctx)
{
	while (ctx->sessions != NULL) {
		/* This removes too the session of the ctx->sessions list. */
		cherry_force_stop_session(ctx->sessions);
	}

	/* All sessions are freed and no more available. */
}

#endif /* CHERRY_SESSION_MANAGER_C */
