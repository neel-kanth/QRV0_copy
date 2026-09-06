/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#pragma once

#include <gtest/gtest.h>
#include <map>
#include <type_traits>

template <typename T>
typename std::enable_if<!std::is_pointer<T>::value>::type compare(const T &t1,
								  const T &t2)
{
	ASSERT_EQ(t1, t2);
}

template <typename T> void compare_iterable(const T &t1, const T &t2)
{
	ASSERT_EQ(std::size(t1), std::size(t2));
	for (auto iter1 = std::begin(t1), last1 = std::end(t1),
		  iter2 = std::begin(t2);
	     iter1 != last1; ++iter1, ++iter2) {
		compare(*iter1, *iter2);
	}
}

template <typename First, typename Second>
void compare(const std::pair<First, Second> &p1,
	     const std::pair<First, Second> &p2)
{
	compare(p1.first, p2.first);
	compare(p1.second, p2.second);
}

template <class K, class T, class C, class A>
void compare(const std::map<K, T, C, A> &m1, const std::map<K, T, C, A> &m2)
{
	compare_iterable(m1, m2);
}
