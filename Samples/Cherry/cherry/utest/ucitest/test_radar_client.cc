/*
 * Implementation for radar client functions
 *
 * SPDX-FileCopyrightText: Copyright (c) 2025 Qorvo, Inc.
 * SPDX-License-Identifier: LicenseRef-QORVO-2
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "cherry_radar_client.h"

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

class RadarSweepBase {
    public:
	RadarSweepBase()
	{
	}

	virtual bool is_truncated() const
	{
		return false;
	}

	void to_uci(struct uci_message_builder *builder) const
	{
		serialize(builder);
	}

	virtual void check(const struct cherry_radar_sweep &sweep) const = 0;

    protected:
	virtual void serialize(struct uci_message_builder *builder) const = 0;

    protected:
	friend class RadarSweepTruncated;
};

class RadarSweepTruncated : public RadarSweepBase {
    public:
	RadarSweepTruncated(const RadarSweepBase &_s, size_t _truncate)
		: RadarSweepBase()
		, s(_s)
		, truncate(_truncate)
	{
	}

	bool is_truncated() const override
	{
		return true;
	}

	void check(const struct cherry_radar_sweep &sweep) const override
	{
		assert(false);
	}

    private:
	void serialize(struct uci_message_builder *builder) const override
	{
		s.serialize(builder);
		builder->message->total_len -= truncate;
		builder->message->size -= truncate;
	}

    private:
	const RadarSweepBase &s;
	size_t truncate;
};

class RadarSweep : public RadarSweepBase {
    public:
	RadarSweep(uint32_t _sequence_number, uint32_t _timestamp,
		   uint8_t _vendor_length, uint8_t *_vendor_data,
		   uint8_t *_samples, uint16_t _samples_length)
		: RadarSweepBase()
		, sequence_number(_sequence_number)
		, timestamp(_timestamp)
		, vendor_length(_vendor_length)
		, vendor_data(_vendor_data)
		, samples(_samples)
		, samples_length(_samples_length)
	{
	}

	void serialize(struct uci_message_builder *builder) const override
	{
		uci_message_put_32bit(builder, sequence_number);
		uci_message_put_32bit(builder, timestamp);
		uci_message_put_8bit(builder, vendor_length);
		if (vendor_length)
			uci_message_put(builder, vendor_data, vendor_length);
		if (samples_length)
			uci_message_put(builder, samples, samples_length);
	}

	void check(const struct cherry_radar_sweep &sweep) const override
	{
		ASSERT_EQ(sweep.sequence_number, sequence_number);
		ASSERT_EQ(sweep.timestamp, timestamp);
		ASSERT_EQ(sweep.vendor_data_len, vendor_length);
		ASSERT_EQ(sweep.data_fragments[0].size, samples_length);
		if (vendor_length) {
			EXPECT_TRUE(0 == std::memcmp(sweep.vendor_data,
						     vendor_data,
						     vendor_length));
		}
		if (samples_length) {
			EXPECT_TRUE(0 ==
				    std::memcmp(sweep.data_fragments[0].data,
						samples, samples_length));
		}
	}

    private:
	uint32_t sequence_number;
	uint32_t timestamp;
	uint8_t vendor_length;
	uint8_t *vendor_data;
	uint8_t *samples;
	uint16_t samples_length;
};

class RadarNtf {
    public:
	typedef std::vector<const RadarSweepBase *> SweepList;

    public:
	RadarNtf() = default;

	RadarNtf(uint32_t _session_handle, uint8_t _status_code,
		 uint8_t _data_type, uint8_t _number_of_sweeps,
		 uint8_t _samples_per_sweep, uint8_t _bits_per_sample,
		 int16_t _sweep_offset, uint16_t _sweep_data_size,
		 std::initializer_list<const RadarSweepBase *> _sweeps = {})
		: session_handle(_session_handle)
		, status_code(_status_code)
		, data_type(_data_type)
		, number_of_sweeps(_number_of_sweeps)
		, samples_per_sweep(_samples_per_sweep)
		, bits_per_sample(_bits_per_sample)
		, sweep_offset(_sweep_offset)
		, sweep_data_size(_sweep_data_size)
		, sweeps(_sweeps)
	{
	}

	void add(const RadarSweep &s)
	{
		sweeps.push_back(&s);
	}

	struct uci_blk *to_uci(struct uci *uci) const
	{
		const uint16_t mt_dpf = UCI_MT_DPF(UCI_MESSAGE_TYPE_DATA,
						   UCI_MESSAGE_DPF_RADAR);
		struct uci_message_builder builder =
			UCI_MESSAGE_BUILDER_INITIALIZER(uci);
		uci_message_put_32bit(&builder, session_handle);
		uci_message_put_8bit(&builder, status_code);
		uci_message_put_8bit(&builder, data_type);
		uci_message_put_8bit(&builder, number_of_sweeps);
		uci_message_put_8bit(&builder, samples_per_sweep);
		uci_message_put_8bit(&builder, bits_per_sample);
		uci_message_put_16bit(&builder, sweep_offset);
		uci_message_put_16bit(&builder, sweep_data_size);

		for (const RadarSweepBase *s : sweeps) {
			s->to_uci(&builder);
		}

		builder.message->total_len -= truncate;
		builder.message->size -= truncate;

		uci_blk_put_data_header(builder.message, mt_dpf,
					builder.message->total_len);

		return builder.message;
	}

    public:
	uint32_t session_handle;
	uint8_t status_code;
	uint8_t data_type;
	uint8_t number_of_sweeps;
	uint8_t samples_per_sweep;
	uint8_t bits_per_sample;
	int16_t sweep_offset;
	uint16_t sweep_data_size;
	SweepList sweeps;

	int8_t truncate = 0;
	bool bad_data = false;
};

static uint8_t radar_vendor_data[] = { 0x01, 0x02, 0x03, 0x04,
				       0x05, 0x06, 0x07, 0x08 };
static uint8_t radar_sample1[60] = {
	0x1,  0x21, 0x1,  0x1,	0x1,  0x1,  0x2,  0x2,	0x2,  0x2,  0x3,  0x3,
	0x4,  0x44, 0x4,  0x54, 0x4,  0x5,  0x5,  0x5,	0x5,  0x6,  0x6,  0x6,
	0x6,  0x6,  0x60, 0x7,	0x7,  0xf,  0xff, 0xf4, 0xba, 0xab, 0xcc, 0xdd,
	0xde, 0xca, 0x30, 0x20, 0x40, 0x50, 0xdb, 0xbd, 0x2f, 0x2a, 0x3c, 0x3f,
	0x22, 0x33, 0x44, 0x44, 0x00, 0xaa, 0xbb, 0xfe, 0x25, 0x26, 0x9,  0x30
};

static const RadarSweep sweep1(0x1, 0xff005533, 0, NULL, radar_sample1,
			       sizeof(radar_sample1));

static const RadarSweep sweep2(0x1, 0xff005533, sizeof(radar_vendor_data),
			       radar_vendor_data, radar_sample1,
			       sizeof(radar_sample1));

static const RadarSweepTruncated bad_sweep(sweep1, 3);

//Test suite
class TestRadarClient : public ::testing::Test {
    protected:
	void SetUp() override
	{
		mock_alloc.implicit_call();
		ASSERT_EQ(uci_init(&uci, &simple_allocator, true), 0);
		ASSERT_EQ(uci_transport_attach(&uci, &transport), 0);
		ASSERT_EQ(cherry_uci_client_radar_open(
				  &context, &uci, NULL,
				  radar_handler_client_radar_ntf_cb),
			  0);
	}
	void TearDown() override
	{
		ASSERT_TRUE(Mock::VerifyAndClearExpectations(&transport));
		uci_transport_detach(&uci);
		cherry_uci_client_radar_close(context);
		uci_uninit(&uci);
	}

	void send_ntf(const RadarNtf &_ntf)
	{
		ntf = &_ntf;
		ntf_received = false;
		transport.SendNotif(_ntf.to_uci(&uci));
		ntf = nullptr;
		if (ntf_received)
			return;

		/* Notification has not been receveived, it is the case when notification or an
		 * sweep is truncated or bad data. */
		size_t nb_truncated_sweep = 0;
		for (auto *s : _ntf.sweeps) {
			if (s->is_truncated())
				++nb_truncated_sweep;
		}
		ASSERT_TRUE(_ntf.truncate != 0 || nb_truncated_sweep != 0 ||
			    _ntf.bad_data);
	}

	struct uci uci;
	struct cherry_radar_context *context = NULL;
	StrictMock<MockUciTransport> transport;
	MockQmalloc mock_alloc;

    public:
	static void radar_handler_client_radar_ntf_cb(
		const struct cherry_uci_radar_ntf *report, void *user_data)
	{
		assert(ntf);

		ntf_received = true;

		EXPECT_EQ(report->session_handle, ntf->session_handle);
		EXPECT_EQ(report->data->status, ntf->status_code);

		if (report->data->status ==
		    CHERRY_RADAR_REPORT_STATUS_SUCCESS) {
			EXPECT_EQ(report->data->n_sweeps,
				  ntf->number_of_sweeps);
			EXPECT_EQ(report->data->samples_per_sweep,
				  ntf->samples_per_sweep);
			EXPECT_EQ(report->data->sample_size,
				  ntf->bits_per_sample);

			auto iter = ntf->sweeps.begin();
			auto last = ntf->sweeps.end();

			for (int idx = 0; idx < report->data->n_sweeps;
			     idx++, ++iter) {
				(*iter)->check(report->data->sweeps[idx]);
			}

			EXPECT_EQ(iter, last);
		}

		/* Free allocated data. */
		cherry_uci_client_radar_free_data_report(report->data);
		cherry_uci_client_radar_free_base_report(
			(struct cherry_uci_radar_ntf *)report);
	}

	static const RadarNtf *ntf;
	static bool ntf_received;
};

const RadarNtf *TestRadarClient::ntf = nullptr;
bool TestRadarClient::ntf_received;

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Nominal case status ok
 **/
TEST_F(TestRadarClient, test_nominal_ntf_data_status_ok)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Nominal case status ok with vendor data
 **/
TEST_F(TestRadarClient, test_nominal_ntf_data_vendor)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep2 });

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Nominal case status ko
 **/
TEST_F(TestRadarClient, test_nominal_ntf_data_status_ko)
{
	RadarNtf ntf(0x01020304, 0x01, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case bad status
 **/
TEST_F(TestRadarClient, test_nominal_ntf_bad_status)
{
	RadarNtf ntf(0x01020304, 0x02, 0x00, 0x00, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });
	ntf.bad_data = true;

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case bad sample size
 **/
TEST_F(TestRadarClient, test_nominal_ntf_bad_sample_size)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x00, 10, 0x3, 0, 9 + 60,
		     { &sweep1 });
	ntf.bad_data = true;

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case status error
 **/
TEST_F(TestRadarClient, test_nominal_ntf_data_error)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x00, 10, 0x1, 0, 9 + 60,
		     { &bad_sweep });

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case status no enough data
 **/
TEST_F(TestRadarClient, test_nominal_ntf_no_enough_data1)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });
	ntf.truncate = 1;

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case status no enough data
 **/
TEST_F(TestRadarClient, test_nominal_ntf_no_enough_data2)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });
	ntf.truncate = 61;

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case status no enough data
 **/
TEST_F(TestRadarClient, test_nominal_ntf_no_enough_data3)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });
	ntf.truncate = 65;

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case status no enough data
 **/
TEST_F(TestRadarClient, test_nominal_ntf_no_enough_data4)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });
	ntf.truncate = 69;

	send_ntf(ntf);
}

/**
 * uci_radar_data_handler() - get radar info notification.
 *
 * Testcase_Type :
 *      Error case status no enough data
 **/
TEST_F(TestRadarClient, test_nominal_ntf_no_enough_data5)
{
	RadarNtf ntf(0x01020304, 0x00, 0x00, 0x01, 10, 0x1, 0, 9 + 60,
		     { &sweep1 });
	ntf.truncate = 71;

	send_ntf(ntf);
}
