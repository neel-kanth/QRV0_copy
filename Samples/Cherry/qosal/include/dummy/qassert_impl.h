/*
 * @copyright SPDX-FileCopyrightText: Copyright (c) 2024 Qorvo, Inc.
 *            SPDX-License-Identifier: LicenseRef-QORVO-2
 *
 */
#pragma once

#ifndef QASSERT

#ifdef NDEBUG
/* Empty implementation to supress unused variables compilation error. */
#define QASSERT(cond) (void)(cond);
#endif

#endif /* QASSERT */
