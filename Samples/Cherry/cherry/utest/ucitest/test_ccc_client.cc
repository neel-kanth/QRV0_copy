/*
 * Implementation for ccc client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_ccc_client.h"
#include "cherry_session_client.h"

#include <qmalloc.h>
#include <uci/uci.h>
#include <uci/uci_message.h>
#include <uci/uci_spec_fira.h>
#include <uci/uci_spec_qorvo.h>
#include <uci/uci_unit_converter.h>
#include <uci_internal.h>
}

#include "mock_qmalloc.hh"
#include "mock_uci_allocator.h"
#include "mock_uci_transport.h"

#include <cstring>

using testing::InSequence;
using testing::Mock;
using testing::Return;
using testing::StrictMock;

static struct uci_blk *simple_acquire(struct uci_allocator *allocator,
				      size_t size_hint, uint8_t flags_hint)
{
	struct uci_blk *p;

	p = (struct uci_blk *)qmalloc(sizeof(*p) + UCI_MAX_PACKET_SIZE);
	if (p) {
		p->data = (uint8_t *)&p[1];
		p->size = UCI_MAX_PACKET_SIZE;
	}
	return p;
}

static void simple_release(struct uci_allocator *allocator,
			   struct uci_blk *packet)
{
	qfree(packet);
}

static struct uci_allocator_ops simple_allocator_ops = {
	.alloc = simple_acquire,
	.free = simple_release,
};

static struct uci_allocator simple_allocator = { .ops = &simple_allocator_ops };

class CccMeasurement {
    public:
	CccMeasurement()
	{
	}

	virtual bool is_unknown() const
	{
		return false;
	}

	virtual bool is_truncated() const
	{
		return false;
	}

	void to_uci(struct uci_message_builder *builder) const
	{
		serialize(builder);
	}

	virtual void
	check(const struct cherry_ccc_session_controller_measurements
		      &measurement) const = 0;
	virtual void
	check(const struct cherry_ccc_session_controlee_measurements
		      &measurement) const = 0;

    protected:
	virtual void serialize(struct uci_message_builder *builder) const = 0;

	friend class CccMeasurementTruncated;
};

class CccMeasurementTruncated : public CccMeasurement {
    public:
	CccMeasurementTruncated(const CccMeasurement &_m, size_t _truncate)
		: CccMeasurement()
		, m(_m)
		, truncate(_truncate)
	{
	}

	bool is_truncated() const override
	{
		return true;
	}

	void check(const struct cherry_ccc_session_controller_measurements
			   &measurement) const override
	{
		assert(false);
	}

	void check(const struct cherry_ccc_session_controlee_measurements
			   &measurement) const override
	{
		assert(false);
	}

    private:
	void serialize(struct uci_message_builder *builder) const override
	{
		m.serialize(builder);
		builder->message->total_len -= truncate;
		builder->message->size -= truncate;
	}

    private:
	const CccMeasurement &m;
	size_t truncate;
};

class CccMeasurementController : public CccMeasurement {
    public:
	CccMeasurementController(enum fira_status _frame_status,
				 uint8_t _slot_index, uint8_t _rssi,
				 uint16_t _rr_index, uint32_t _sts_index,
				 uint8_t _ranging_round)
		: CccMeasurement()
		, frame_status(_frame_status)
		, slot_index(_slot_index)
		, rssi(_rssi)
		, rr_index(_rr_index)
		, sts_index(_sts_index)
		, ranging_round(_ranging_round)
	{
	}

	void serialize(struct uci_message_builder *builder) const override
	{
		uint8_t rfu_data[12] = { 0 };

		uci_message_put_8bit(builder, frame_status);
		uci_message_put_8bit(builder, slot_index);
		uci_message_put_16bit(builder, rr_index);
		uci_message_put_32bit(builder, sts_index);
		uci_message_put_32bit(builder, ranging_round);
		uci_message_put(builder, rfu_data, sizeof(rfu_data));
	}

	void check(const struct cherry_ccc_session_controller_measurements
			   &measurement) const override
	{
		ASSERT_EQ(measurement.frame_status,
			  cherry_session_frame_status_to_cherry_format(
				  frame_status));
		ASSERT_EQ(measurement.slot_index, slot_index);
		ASSERT_EQ(measurement.rr_index, rr_index);
		ASSERT_EQ(measurement.sts_index, sts_index);
		ASSERT_EQ(measurement.ranging_round, ranging_round);
	}

	void check(const struct cherry_ccc_session_controlee_measurements
			   &measurement) const override
	{
	}

    private:
	enum fira_status frame_status;
	uint8_t slot_index;
	uint8_t rssi;
	uint16_t rr_index;
	uint32_t sts_index;
	uint8_t ranging_round;
};

class CccMeasurementControlee : public CccMeasurement {
    public:
	CccMeasurementControlee(enum fira_status _frame_status,
				uint8_t _slot_index, uint16_t _rr_index,
				uint32_t _sts_index, uint16_t _distance_cm,
				uint8_t _uncertainty_anchor,
				uint8_t _uncertainty_initiator,
				uint8_t _ranging_round)
		: CccMeasurement()
		, frame_status(_frame_status)
		, slot_index(_slot_index)
		, rr_index(_rr_index)
		, sts_index(_sts_index)
		, distance_cm(_distance_cm)
		, uncertainty_anchor(_uncertainty_anchor)
		, uncertainty_initiator(_uncertainty_initiator)
		, ranging_round(_ranging_round)
	{
	}

	void serialize(struct uci_message_builder *builder) const override
	{
		uint8_t rfu_data[12] = { 0 };

		uci_message_put_8bit(builder, frame_status);
		uci_message_put_8bit(builder, slot_index);
		uci_message_put_16bit(builder, rr_index);
		uci_message_put_32bit(builder, sts_index);
		uci_message_put_16bit(builder, distance_cm);
		uci_message_put_8bit(builder, uncertainty_anchor);
		uci_message_put_8bit(builder, uncertainty_initiator);
		uci_message_put_8bit(builder, ranging_round);
		uci_message_put(builder, rfu_data, sizeof(rfu_data));
	}

	void check(const struct cherry_ccc_session_controlee_measurements
			   &measurement) const override
	{
		ASSERT_EQ(measurement.frame_status,
			  cherry_session_frame_status_to_cherry_format(
				  frame_status));
		ASSERT_EQ(measurement.slot_index, slot_index);
		ASSERT_EQ(measurement.rr_index, rr_index);
		ASSERT_EQ(measurement.sts_index, sts_index);
		ASSERT_EQ(measurement.distance_cm, distance_cm);
		ASSERT_EQ(measurement.uncertainty_anchor, uncertainty_anchor);
		ASSERT_EQ(measurement.uncertainty_initiator,
			  uncertainty_initiator);
		ASSERT_EQ(measurement.ranging_round, ranging_round);
	}

	void check(const struct cherry_ccc_session_controller_measurements
			   &measurement) const override
	{
	}

    private:
	enum fira_status frame_status;
	uint8_t slot_index;
	uint16_t rr_index;
	uint32_t sts_index;
	uint16_t distance_cm;
	uint8_t uncertainty_anchor;
	uint8_t uncertainty_initiator;
	uint8_t ranging_round;
};

class CccNtf {
    public:
	typedef std::vector<const CccMeasurement *> ReportList;

    public:
	CccNtf() = default;

	CccNtf(uint32_t _sequence_number, uint32_t _session_handle,
	       enum fira_ranging_data_attrs_ranging_measurement_type _type,
	       std::initializer_list<const CccMeasurement *> _measurements = {})
		: sequence_number(_sequence_number)
		, session_handle(_session_handle)
		, type(_type)
		, measurements(_measurements)
	{
	}

	void add(const CccMeasurement &e)
	{
		measurements.push_back(&e);
	}

	struct uci_blk *to_uci(struct uci *uci) const
	{
		const uint16_t mt_gid_oid = UCI_MT_GID_OID(
			UCI_MESSAGE_TYPE_NOTIFICATION, UCI_GID_SESSION_CONTROL,
			UCI_OID_SESSION_INFO);
		struct uci_message_builder builder =
			UCI_MESSAGE_BUILDER_INITIALIZER(uci);
		uci_message_put_32bit(&builder, sequence_number);
		uci_message_put_32bit(&builder, session_handle);
		uci_message_put_8bit(&builder, 0); /* RFU. */
		uci_message_put_32bit(&builder,
				      0); /* Current Ranging Interval. */
		uci_message_put_8bit(&builder, type);
		uci_message_put_8bit(&builder, 0); /* RFU. */
		uci_message_put_8bit(&builder,
				     0); /* MAC Addressing Mode Indicator. */
		uci_message_put_32bit(
			&builder, 0); /* Session ID of HUS Primary Session. . */
		uci_message_put_32bit(&builder, 0); /* RFU. */
		uci_message_put_8bit(&builder, measurements.size());

		for (const CccMeasurement *e : measurements) {
			e->to_uci(&builder);
		}

		builder.message->total_len -= truncate;
		builder.message->size -= truncate;

		uci_blk_put_control_header(builder.message, mt_gid_oid,
					   builder.message->total_len);

		return builder.message;
	}

    public:
	uint32_t sequence_number;
	uint32_t session_handle;
	enum fira_ranging_data_attrs_ranging_measurement_type type;

	ReportList measurements;

	int8_t truncate = 0;
};

static const CccMeasurementController meas_controller1(FIRA_STATUS_OK, 0x01,
						       0x02, 0x03, 0x04, 0x01);
static const CccMeasurementController
	meas_controller2(FIRA_STATUS_RANGING_RX_TIMEOUT, 0x02, 0x03, 0x04, 0x05,
			 0x00);
static const CccMeasurementControlee meas_controlee1(FIRA_STATUS_OK, 0x01, 0x02,
						     0x03, 0x04, 0x05, 0x06,
						     0x01);
static const CccMeasurementControlee
	meas_controlee2(FIRA_STATUS_RANGING_RX_TIMEOUT, 0x02, 0x03, 0x04, 0x05,
			0x06, 0x07, 0x00);
static const CccMeasurementTruncated truncated_meas_controller(meas_controller1,
							       20);
static const CccMeasurementTruncated truncated_meas_controlee(meas_controlee1,
							      24);

static void session_status_cb(const struct session_status_ntf *ntf,
			      void *user_data)
{
}

//Test suite
class TestCccClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_session_open(
				  &context, &uci, NULL, session_status_cb,
				  ccc_handler_client_ccc_ntf_cb),
			  0);
	}
	void TearDown() override
	{
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		cherry_uci_client_session_close(context);
		uci_uninit(&uci);
	}

	void send_ntf(const CccNtf &_ntf)
	{
		ntf = &_ntf;
		ntf_received = false;
		transport.SendNotif(_ntf.to_uci(&uci));
		ntf = nullptr;
		if (ntf_received)
			return;

		/* Notification has not been receveived, it is the case when notification or an
		 * measurement is truncated. */
		size_t nb_truncated_measurement = 0;
		for (auto *e : _ntf.measurements)
			if (e->is_truncated())
				++nb_truncated_measurement;
		ASSERT_TRUE(_ntf.truncate != 0 ||
			    nb_truncated_measurement != 0);
	}

	struct uci uci;
	struct cherry_session_context *context = NULL;
	StrictMock<MockUciTransport> transport;
	MockQmalloc mock_alloc;

    public:
	static void
	ccc_handler_client_ccc_ntf_cb(const struct session_ranging_data *data)
	{
		struct cherry_ccc_controller_session_report *controller_report =
			NULL;
		struct cherry_ccc_controlee_session_report *controlee_report =
			NULL;
		struct cherry_ccc_session_controller_measurements
			*controller_measurement;
		struct cherry_ccc_session_controlee_measurements
			*controlee_measurement;

		assert(ntf);

		ntf_received = true;

		EXPECT_EQ(data->session_handle, ntf->session_handle);
		EXPECT_EQ(data->sequence_number, ntf->sequence_number);
		EXPECT_EQ(data->type, ntf->type);

		switch (data->type) {
		case FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER:
			controller_report = (struct cherry_ccc_controller_session_report
						     *)
				qmalloc(sizeof(
					struct cherry_ccc_controller_session_report));
			if (!controller_report)
				return;

			controller_report->measurements = NULL;

			if (cherry_uci_client_parse_ccc_controller_measurements(
				    data, controller_report) != QERR_SUCCESS) {
				cherry_uci_client_ccc_free_data_controller_report(
					controller_report);
				return;
			}

			break;
		case FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE:
			controlee_report = (struct cherry_ccc_controlee_session_report
						    *)
				qmalloc(sizeof(
					struct cherry_ccc_controlee_session_report));
			if (!controlee_report)
				return;

			controlee_report->measurements = NULL;

			if (cherry_uci_client_parse_ccc_controlee_measurements(
				    data, controlee_report) != QERR_SUCCESS) {
				cherry_uci_client_ccc_free_data_controlee_report(
					controlee_report);
				return;
			}
			break;
		default:
			break;
		}

		auto iter = ntf->measurements.begin();
		auto last = ntf->measurements.end();

		switch (data->type) {
		case FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER:
			for (controller_measurement =
				     controller_report->measurements;
			     controller_measurement;
			     controller_measurement =
				     controller_measurement->next,
			    ++iter) {
				/* Skip unknown measurements. */
				for (; iter != last && (*iter)->is_unknown();
				     ++iter)
					;
				if (iter == last)
					break;

				(*iter)->check(*controller_measurement);
			}
			EXPECT_EQ(controller_measurement, nullptr);
			break;
		case FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE:
			for (controlee_measurement =
				     controlee_report->measurements;
			     controlee_measurement;
			     controlee_measurement =
				     controlee_measurement->next,
			    ++iter) {
				/* Skip unknown measurements. */
				for (; iter != last && (*iter)->is_unknown();
				     ++iter)
					;
				if (iter == last)
					break;

				(*iter)->check(*controlee_measurement);
			}
			EXPECT_EQ(controlee_measurement, nullptr);
			break;
		default:
			break;
		}

		EXPECT_EQ(iter, last);

		/* Free allocated data. */
		if (data->type ==
		    FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER)
			cherry_uci_client_ccc_free_data_controller_report(
				controller_report);
		else
			cherry_uci_client_ccc_free_data_controlee_report(
				controlee_report);
	}
	static const CccNtf *ntf;
	static bool ntf_received;
};

const CccNtf *TestCccClient::ntf = nullptr;
bool TestCccClient::ntf_received;

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controller notification.
 *
 * Testcase_Type :
 *      Nominal case for controller
 **/
TEST_F(TestCccClient, test_nominal_ntf_controller)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER,
		{
			&meas_controller1,
		});

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controlee notification.
 *
 * Testcase_Type :
 *      Nominal case for controlee
 **/
TEST_F(TestCccClient, test_nominal_ntf_controlee)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE,
		{
			&meas_controlee1,
		});

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controller notification.
 *
 * Testcase_Type :
 *      Nominal case for controller
 **/
TEST_F(TestCccClient, test_nominal_ntf_controller_2)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER,
		{
			&meas_controller1,
			&meas_controller2,
		});

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controlee notification.
 *
 * Testcase_Type :
 *      Nominal case for controlee
 **/
TEST_F(TestCccClient, test_nominal_ntf_controlee_2)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE,
		{
			&meas_controlee1,
			&meas_controlee2,
		});

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controller notification.
 *
 * Testcase_Type :
 *      Too much data for controller
 **/
TEST_F(TestCccClient, test_nominal_ntf_controller_too_much_data)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER,
		{
			&meas_controller1,
		});

	ntf.truncate = -1;

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controlee notification.
 *
 * Testcase_Type :
 *      Too much data for controlee
 **/
TEST_F(TestCccClient, test_nominal_ntf_controlee_too_much_data)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE,
		{
			&meas_controlee1,
		});

	ntf.truncate = -1;

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controller notification.
 *
 * Testcase_Type :
 *      No enough data for controller
 **/
TEST_F(TestCccClient, test_nominal_ntf_controller_no_enough_data1)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER,
		{
			&meas_controller1,
		});

	ntf.truncate = 1;

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controller notification.
 *
 * Testcase_Type :
 *      No enough data for controller
 **/
TEST_F(TestCccClient, test_nominal_ntf_controller_no_enough_data2)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER,
		{
			&meas_controller1,
		});

	ntf.truncate = 21;

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controlee notification.
 *
 * Testcase_Type :
 *      No enough data for controlee
 **/
TEST_F(TestCccClient, test_nominal_ntf_controlee_no_enough_data)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE,
		{
			&meas_controlee1,
		});

	ntf.truncate = 1;

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controller notification.
 *
 * Testcase_Type :
 *      No measurement data for controller
 **/
TEST_F(TestCccClient, test_nominal_ntf_controller_no_measurement)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLLER,
		{
			&truncated_meas_controller,
		});

	send_ntf(ntf);
}

/**
 * uci_rsp_ccc_info_ntf_handler() - get CCC controlee notification.
 *
 * Testcase_Type :
 *      No measurement data for controlee
 **/
TEST_F(TestCccClient, test_nominal_ntf_controlee_no_measurement)
{
	CccNtf ntf(
		0x01020304, 0x05060708,
		FIRA_RANGING_DATA_ATTR_RANGING_MEASUREMENT_TYPE_CCC_CONTROLEE,
		{
			&truncated_meas_controlee,
		});

	send_ntf(ntf);
}
