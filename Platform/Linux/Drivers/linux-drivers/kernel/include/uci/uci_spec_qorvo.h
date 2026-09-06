/*
 * SPDX-FileCopyrightText: Copyright (c) 2020 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2 OR GPL-2.0
 */

#pragma once

/**
 * enum uci_oid_qorvo_ext1 - Opcode identifiers for UCI_GID_QORVO_EXT1.
 *
 * @UCI_OID_QORVO_EXT1_SE_TEST_LOOPBACK:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_CLEAR_DATA_PARTITION:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_GET_BINDING_STATUS:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_DO_BINDING:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_GET_CPLC:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_MANAGE_TEST_RDS:
 *     Set test command.
 * @UCI_OID_QORVO_EXT1_SE_GET_RDS_SEC:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_MANAGE_TEST_RDS_SEC:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_ERASE_RDS_LIST:
 *      To be documented.
 * @UCI_OID_QORVO_EXT1_SE_START_SECURE_CHANNEL:
 *      Open the secure channel.
 *      (only used when CONIFG_SE_SECURE_CHANNEL_LET_OPENED is true).
 * @UCI_OID_QORVO_EXT1_SE_GET_STATUS:
 *      Retrieve state for scp (open/close) and user mode.
 * @UCI_OID_QORVO_EXT1_SE_STOP_SECURE_CHANNEL:
 *      Close the secure channel.
 *      (only used when CONIFG_SE_SECURE_CHANNEL_LET_OPENED is true).
 */
enum uci_oid_qorvo_ext1 {
	UCI_OID_QORVO_EXT1_SE_TEST_LOOPBACK = 0x16,
	UCI_OID_QORVO_EXT1_SE_CLEAR_DATA_PARTITION = 0x17,
	UCI_OID_QORVO_EXT1_SE_GET_BINDING_STATUS = 0x18,
	UCI_OID_QORVO_EXT1_SE_DO_BINDING = 0x19,
	UCI_OID_QORVO_EXT1_SE_GET_CPLC = 0x22,
	UCI_OID_QORVO_EXT1_SE_MANAGE_TEST_RDS = 0x23,
	UCI_OID_QORVO_EXT1_SE_GET_RDS_SEC = 0x24,
	UCI_OID_QORVO_EXT1_SE_MANAGE_TEST_RDS_SEC = 0x25,
	UCI_OID_QORVO_EXT1_SE_ERASE_RDS_LIST = 0x26,
	UCI_OID_QORVO_EXT1_SE_START_SECURE_CHANNEL = 0x27,
	UCI_OID_QORVO_EXT1_SE_GET_STATUS = 0x28,
	UCI_OID_QORVO_EXT1_SE_STOP_SECURE_CHANNEL = 0x29,
};

/**
 * enum uci_oid_qorvo_ext2 - Opcode identifiers for UCI_GID_QORVO_EXT2.
 *
 * @UCI_OID_QORVO_EXT2_TEST_DEBUG:
 *     Opcode for tests additional notification.
 * @UCI_OID_QORVO_EXT2_TEST_TX_CW:
 *     Opcode for continuous wave transmission test.
 * @UCI_OID_QORVO_EXT2_TEST_PLLRF:
 *     Opcode for PLL status test.
 * @UCI_OID_QORVO_EXT2_FIRA_RANGE_DIAGNOSTICS:
 *     Diagnostics notifications.
 * @UCI_OID_QORVO_EXT2_SESSION_GET:
 *     Retrieve all existing sessions.
 * @UCI_OID_QORVO_EXT2_FIRA_SET_ANT_FLEX_CONFIG:
 *     Set FiRa antenna flexibility configuration.
 * @UCI_OID_QORVO_EXT2_FIRA_GET_ANT_FLEX_CONFIG:
 *     Get FiRa antenna flexibility configuration.
 * @UCI_OID_QORVO_EXT2_CCC_SET_ANT_FLEX_CONFIG:
 *     Set ccc antenna flexibility configuration.
 * @UCI_OID_QORVO_EXT2_CCC_GET_ANT_FLEX_CONFIG:
 *     Get ccc antenna flexibility configuration.
 * @UCI_OID_QORVO_EXT2_CORE_PSDU_DUMP:
 *     Dump Notification.
 * @UCI_OID_QORVO_EXT2_CORE_GET_MEM_STATS:
 *	Retrieve UWBS memory statistics.
 * @UCI_OID_QORVO_EXT2_CORE_GET_POWER_STATS:
 *	Retrieve UWB power statistics.
 * @UCI_OID_QORVO_EXT2_CORE_GET_CPU_STATS:
 *	Retrieve UWB cpu statistics.
 * @UCI_OID_QORVO_EXT2_CORE_RESET_CPU_STATS:
 *	Reset UWB cpu statistics.
 * @UCI_OID_QORVO_EXT2_CORE_GET_DEVICE_STATS:
 *	Get UWB device statistics.
 * @UCI_OID_QORVO_EXT2_CORE_READ_REG:
 *	Read UWB register.
 * @UCI_OID_QORVO_EXT2_CORE_ERASE_CERTS:
 *     Erase qorvo certificates.
 * @UCI_OID_QORVO_EXT2_CORE_DEVICE_BOOT:
 *     UWBS boot notification.
 * @UCI_OID_QORVO_EXT2_CORE_QUERY_GPIO_TIMESTAMP:
 *     Query GPIO timestamp data.
 * @UCI_OID_QORVO_EXT2_CORE_SESSION_SCHEDULING_INFO_NTF:
 *     Session scheduling info notification.
 * @UCI_OID_QORVO_EXT2_HW_TEST_MODE_GPIO_CONFIGURE: See sphinx doc.
 * @UCI_OID_QORVO_EXT2_HW_TEST_MODE_GPIO_SET: See sphinx doc.
 * @UCI_OID_QORVO_EXT2_HW_TEST_MODE_GPIO_GET: See sphinx doc.
 * @UCI_OID_QORVO_EXT2_HW_TEST_MODE_CLOCK_OUT_CONFIG: See sphinx doc.
 * @UCI_OID_QORVO_EXT2_HW_TEST_MODE_CLOCK_OUT_CONTROL: See sphinx doc.
 */
enum uci_oid_qorvo_ext2 {
	UCI_OID_QORVO_EXT2_TEST_DEBUG = 0x00,
	UCI_OID_QORVO_EXT2_TEST_TX_CW = 0x01,
	UCI_OID_QORVO_EXT2_TEST_PLLRF = 0x02,
	UCI_OID_QORVO_EXT2_FIRA_RANGE_DIAGNOSTICS = 0x03,
	UCI_OID_QORVO_EXT2_SESSION_GET = 0x07,
	UCI_OID_QORVO_EXT2_FIRA_SET_ANT_FLEX_CONFIG = 0x08,
	UCI_OID_QORVO_EXT2_FIRA_GET_ANT_FLEX_CONFIG = 0x09,
	UCI_OID_QORVO_EXT2_CCC_SET_ANT_FLEX_CONFIG = 0x0a,
	UCI_OID_QORVO_EXT2_CCC_GET_ANT_FLEX_CONFIG = 0x0b,
	UCI_OID_QORVO_EXT2_CORE_PSDU_DUMP = 0x22,
	UCI_OID_QORVO_EXT2_CORE_GET_MEM_STATS = 0x23,
	UCI_OID_QORVO_EXT2_CORE_GET_POWER_STATS = 0x24,
	UCI_OID_QORVO_EXT2_CORE_GET_CPU_STATS = 0x25,
	UCI_OID_QORVO_EXT2_CORE_RESET_CPU_STATS = 0x26,
	UCI_OID_QORVO_EXT2_CORE_GET_DEVICE_STATS = 0x27,
	UCI_OID_QORVO_EXT2_CORE_READ_REG = 0x29,
	UCI_OID_QORVO_EXT2_CORE_ERASE_CERTS = 0x30,
	UCI_OID_QORVO_EXT2_CORE_DEVICE_BOOT = 0x31,
	UCI_OID_QORVO_EXT2_CORE_QUERY_GPIO_TIMESTAMP = 0x36,
	UCI_OID_QORVO_EXT2_CORE_SESSION_SCHEDULING_INFO_NTF = 0x3e,
	UCI_OID_QORVO_EXT2_HW_TEST_MODE_GPIO_CONFIGURE = 0x3f,
	UCI_OID_QORVO_EXT2_HW_TEST_MODE_GPIO_SET = 0x40,
	UCI_OID_QORVO_EXT2_HW_TEST_MODE_GPIO_GET = 0x41,
	UCI_OID_QORVO_EXT2_HW_TEST_MODE_CLOCK_OUT_CONFIG = 0x42,
	UCI_OID_QORVO_EXT2_HW_TEST_MODE_CLOCK_OUT_CONTROL = 0x43,
};

/**
 * enum uci_oid_qorvo_calib - Opcode identifiers for UCI_GID_QORVO_CALIB.
 *
 * @UCI_OID_QORVO_CALIB_RESET:
 *     Request to reset the calibration data.
 *
 * TODO: to be removed and merged with UCI_GID_QORVO_MAC.
 */
enum uci_oid_qorvo_calib {
	UCI_OID_QORVO_CALIB_RESET = 0x00,
};

/**
 * enum uci_qorvo_boot_reason - Boot reason sent in QORVO_CORE_DEVICE_BOOT_NTF.
 *
 * @UCI_QORVO_BOOT_REASON_UNKNOWN:
 *     Unknown.
 * @UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET:
 *     Reboot after fatal error.
 */
enum uci_qorvo_boot_reason {
	UCI_QORVO_BOOT_REASON_UNKNOWN = 0,
	UCI_QORVO_BOOT_REASON_FATAL_ERROR_RESET = 1,
};
