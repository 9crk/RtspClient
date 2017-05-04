/*
 * The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) implementation with additional features.
 * Copyright (C) 2017 Belledonne Communications SARL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef RTCP_H
#define RTCP_H

// #include <ortp/port.h>

#define RTCP_MAX_RECV_BUFSIZE 1500

#define RTCP_SENDER_INFO_SIZE 20
#define RTCP_REPORT_BLOCK_SIZE 24
#define RTCP_COMMON_HEADER_SIZE 4
#define RTCP_SSRC_FIELD_SIZE 4

#ifdef __cplusplus
extern "C"{
#endif

/* RTCP common header */

typedef enum {
	RTCP_SR = 200,
	RTCP_RR = 201,
	RTCP_SDES = 202,
	RTCP_BYE = 203,
	RTCP_APP = 204,
	RTCP_RTPFB = 205,
	RTCP_PSFB = 206,
	RTCP_XR = 207
} rtcp_type_t;

typedef struct rtcp_common_header
{
#ifdef ORTP_BIGENDIAN
	uint16_t version:2;
	uint16_t padbit:1;
	uint16_t rc:5;
	uint16_t packet_type:8;
#else
	uint16_t rc:5;
	uint16_t padbit:1;
	uint16_t version:2;
	uint16_t packet_type:8;
#endif
	uint16_t length:16;
} rtcp_common_header_t;

#define rtcp_common_header_set_version(ch,v) (ch)->version=v
#define rtcp_common_header_set_padbit(ch,p) (ch)->padbit=p
#define rtcp_common_header_set_rc(ch,val) (ch)->rc=val
#define rtcp_common_header_set_packet_type(ch,pt) (ch)->packet_type=pt
#define rtcp_common_header_set_length(ch,l)	(ch)->length=htons(l)

#define rtcp_common_header_get_version(ch) ((ch)->version)
#define rtcp_common_header_get_padbit(ch) ((ch)->padbit)
#define rtcp_common_header_get_rc(ch) ((ch)->rc)
#define rtcp_common_header_get_packet_type(ch) ((ch)->packet_type)
#define rtcp_common_header_get_length(ch)	ntohs((ch)->length)


/* RTCP SR or RR  packets */

typedef struct sender_info
{
	uint32_t ntp_timestamp_msw;
	uint32_t ntp_timestamp_lsw;
	uint32_t rtp_timestamp;
	uint32_t senders_packet_count;
	uint32_t senders_octet_count;
} sender_info_t;
#define ORTP_INLINE inline


static ORTP_INLINE uint64_t sender_info_get_ntp_timestamp(const sender_info_t *si) {
  return ((((uint64_t)ntohl(si->ntp_timestamp_msw)) << 32) +
          ((uint64_t) ntohl(si->ntp_timestamp_lsw)));
}
#define sender_info_get_rtp_timestamp(si)	ntohl((si)->rtp_timestamp)
#define sender_info_get_packet_count(si) \
	ntohl((si)->senders_packet_count)
#define sender_info_get_octet_count(si) \
	ntohl((si)->senders_octet_count)


typedef struct report_block
{
	uint32_t ssrc;
	uint32_t fl_cnpl;/*fraction lost + cumulative number of packet lost*/
	uint32_t ext_high_seq_num_rec; /*extended highest sequence number received */
	uint32_t interarrival_jitter;
	uint32_t lsr; /*last SR */
	uint32_t delay_snc_last_sr; /*delay since last sr*/
} report_block_t;

static ORTP_INLINE uint32_t report_block_get_ssrc(const report_block_t * rb) {
	return ntohl(rb->ssrc);
}
static ORTP_INLINE uint32_t report_block_get_high_ext_seq(const report_block_t * rb) {
	return ntohl(rb->ext_high_seq_num_rec);
}
static ORTP_INLINE uint32_t report_block_get_interarrival_jitter(const report_block_t * rb) {
	return ntohl(rb->interarrival_jitter);
}

static ORTP_INLINE uint32_t report_block_get_last_SR_time(const report_block_t * rb) {
	return ntohl(rb->lsr);
}
static ORTP_INLINE uint32_t report_block_get_last_SR_delay(const report_block_t * rb) {
	return ntohl(rb->delay_snc_last_sr);
}
static ORTP_INLINE uint32_t report_block_get_fraction_lost(const report_block_t * rb) {
	return (ntohl(rb->fl_cnpl)>>24);
}
static ORTP_INLINE int32_t report_block_get_cum_packet_lost(const report_block_t * rb){
	int cum_loss = ntohl(rb->fl_cnpl);
	if (((cum_loss>>23)&1)==0)
		return 0x00FFFFFF & cum_loss;
	else
		return 0xFF000000 | (cum_loss-0xFFFFFF-1);
}

static ORTP_INLINE void report_block_set_fraction_lost(report_block_t * rb, int fl){
	rb->fl_cnpl = htonl( (ntohl(rb->fl_cnpl) & 0xFFFFFF) | (fl&0xFF)<<24);
}

static ORTP_INLINE void report_block_set_cum_packet_lost(report_block_t * rb, int64_t cpl) {
	uint32_t clamp = (uint32_t)((1<<24) + ((cpl>=0) ? (cpl>0x7FFFFF?0x7FFFFF:cpl) : (-cpl>0x800000?-0x800000:cpl)));

	rb->fl_cnpl=htonl(
			(ntohl(rb->fl_cnpl) & 0xFF000000) |
			(cpl >= 0 ? clamp&0x7FFFFF : clamp|0x800000)
		);
}

/* SDES packets */

typedef enum {
	RTCP_SDES_END = 0,
	RTCP_SDES_CNAME = 1,
	RTCP_SDES_NAME = 2,
	RTCP_SDES_EMAIL = 3,
	RTCP_SDES_PHONE = 4,
	RTCP_SDES_LOC = 5,
	RTCP_SDES_TOOL = 6,
	RTCP_SDES_NOTE = 7,
	RTCP_SDES_PRIV = 8,
	RTCP_SDES_MAX = 9
} rtcp_sdes_type_t;

typedef struct sdes_chunk
{
	uint32_t csrc;
} sdes_chunk_t;


#define sdes_chunk_get_csrc(c)	ntohl((c)->csrc)

typedef struct sdes_item
{
	uint8_t item_type;
	uint8_t len;
	char content[1];
} sdes_item_t;

#define RTCP_SDES_MAX_STRING_SIZE 255
#define RTCP_SDES_ITEM_HEADER_SIZE 2
#define RTCP_SDES_CHUNK_DEFAULT_SIZE 1024
#define RTCP_SDES_CHUNK_HEADER_SIZE (sizeof(sdes_chunk_t))

/* RTCP bye packet */

typedef struct rtcp_bye_reason
{
	uint8_t len;
	char content[1];
} rtcp_bye_reason_t;

typedef struct rtcp_bye
{
	rtcp_common_header_t ch;
	uint32_t ssrc[1];  /* the bye may contain several ssrc/csrc */
} rtcp_bye_t;
#define RTCP_BYE_HEADER_SIZE sizeof(rtcp_bye_t)
#define RTCP_BYE_REASON_MAX_STRING_SIZE 255


/* RTCP XR packet */

#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_STD ((1 << 7) | (1 << 6))
#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_ENH (1 << 7)
#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_DIS (1 << 6)
#define RTCP_XR_VOIP_METRICS_CONFIG_PLC_UNS 0
#define RTCP_XR_VOIP_METRICS_CONFIG_JBA_ADA ((1 << 5) | (1 << 4))
#define RTCP_XR_VOIP_METRICS_CONFIG_JBA_NON (1 << 5)
#define RTCP_XR_VOIP_METRICS_CONFIG_JBA_UNK 0

typedef enum {
	RTCP_XR_LOSS_RLE = 1,
	RTCP_XR_DUPLICATE_RLE = 2,
	RTCP_XR_PACKET_RECEIPT_TIMES = 3,
	RTCP_XR_RCVR_RTT = 4,
	RTCP_XR_DLRR = 5,
	RTCP_XR_STAT_SUMMARY = 6,
	RTCP_XR_VOIP_METRICS = 7
} rtcp_xr_block_type_t;

typedef struct rtcp_xr_header {
	rtcp_common_header_t ch;
	uint32_t ssrc;
} rtcp_xr_header_t;

typedef struct rtcp_xr_generic_block_header {
	uint8_t bt;
	uint8_t flags;
	uint16_t length;
} rtcp_xr_generic_block_header_t;

typedef struct rtcp_xr_rcvr_rtt_report_block {
	rtcp_xr_generic_block_header_t bh;
	uint32_t ntp_timestamp_msw;
	uint32_t ntp_timestamp_lsw;
} rtcp_xr_rcvr_rtt_report_block_t;

typedef struct rtcp_xr_dlrr_report_subblock {
	uint32_t ssrc;
	uint32_t lrr;
	uint32_t dlrr;
} rtcp_xr_dlrr_report_subblock_t;

typedef struct rtcp_xr_dlrr_report_block {
	rtcp_xr_generic_block_header_t bh;
	rtcp_xr_dlrr_report_subblock_t content[1];
} rtcp_xr_dlrr_report_block_t;

typedef struct rtcp_xr_stat_summary_report_block {
	rtcp_xr_generic_block_header_t bh;
	uint32_t ssrc;
	uint16_t begin_seq;
	uint16_t end_seq;
	uint32_t lost_packets;
	uint32_t dup_packets;
	uint32_t min_jitter;
	uint32_t max_jitter;
	uint32_t mean_jitter;
	uint32_t dev_jitter;
	uint8_t min_ttl_or_hl;
	uint8_t max_ttl_or_hl;
	uint8_t mean_ttl_or_hl;
	uint8_t dev_ttl_or_hl;
} rtcp_xr_stat_summary_report_block_t;

typedef struct rtcp_xr_voip_metrics_report_block {
	rtcp_xr_generic_block_header_t bh;
	uint32_t ssrc;
	uint8_t loss_rate;
	uint8_t discard_rate;
	uint8_t burst_density;
	uint8_t gap_density;
	uint16_t burst_duration;
	uint16_t gap_duration;
	uint16_t round_trip_delay;
	uint16_t end_system_delay;
	uint8_t signal_level;
	uint8_t noise_level;
	uint8_t rerl;
	uint8_t gmin;
	uint8_t r_factor;
	uint8_t ext_r_factor;
	uint8_t mos_lq;
	uint8_t mos_cq;
	uint8_t rx_config;
	uint8_t reserved2;
	uint16_t jb_nominal;
	uint16_t jb_maximum;
	uint16_t jb_abs_max;
} rtcp_xr_voip_metrics_report_block_t;

#define MIN_RTCP_XR_PACKET_SIZE (sizeof(rtcp_xr_header_t) + 4)

/* RTCP FB packet */
typedef enum {
	RTCP_RTPFB_NACK = 1,
	RTCP_RTPFB_TMMBR = 3,
	RTCP_RTPFB_TMMBN = 4
} rtcp_rtpfb_type_t;

typedef enum {
	RTCP_PSFB_PLI = 1,
	RTCP_PSFB_SLI = 2,
	RTCP_PSFB_RPSI = 3,
	RTCP_PSFB_FIR = 4,
	RTCP_PSFB_AFB = 15
} rtcp_psfb_type_t;

typedef struct rtcp_fb_header {
	uint32_t packet_sender_ssrc;
	uint32_t media_source_ssrc;
} rtcp_fb_header_t;

typedef struct rtcp_fb_generic_nack_fci {
	uint16_t pid;
	uint16_t blp;
} rtcp_fb_generic_nack_fci_t;

#define rtcp_fb_generic_nack_fci_get_pid(nack) ntohs((nack)->pid)
#define rtcp_fb_generic_nack_fci_set_pid(nack, value) ((nack)->pid) = htons(value)
#define rtcp_fb_generic_nack_fci_get_blp(nack) ntohs((nack)->blp)
#define rtcp_fb_generic_nack_fci_set_blp(nack, value) ((nack)->blp) = htons(value)

typedef struct rtcp_fb_tmmbr_fci {
	uint32_t ssrc;
	uint32_t value;
} rtcp_fb_tmmbr_fci_t;

#define rtcp_fb_tmmbr_fci_get_ssrc(tmmbr) ntohl((tmmbr)->ssrc)
#define rtcp_fb_tmmbr_fci_get_mxtbr_exp(tmmbr) \
	((uint8_t)((ntohl((tmmbr)->value) >> 26) & 0x0000003F))
#define rtcp_fb_tmmbr_fci_set_mxtbr_exp(tmmbr, mxtbr_exp) \
	((tmmbr)->value) = htonl((ntohl((tmmbr)->value) & 0x03FFFFFF) | (((mxtbr_exp) & 0x0000003F) << 26))
#define rtcp_fb_tmmbr_fci_get_mxtbr_mantissa(tmmbr) \
	((uint32_t)((ntohl((tmmbr)->value) >> 9) & 0x0001FFFF))
#define rtcp_fb_tmmbr_fci_set_mxtbr_mantissa(tmmbr, mxtbr_mantissa) \
	((tmmbr)->value) = htonl((ntohl((tmmbr)->value) & 0xFC0001FF) | (((mxtbr_mantissa) & 0x0001FFFF) << 9))
#define rtcp_fb_tmmbr_fci_get_measured_overhead(tmmbr) \
	((uint16_t)(ntohl((tmmbr)->value) & 0x000001FF))
#define rtcp_fb_tmmbr_fci_set_measured_overhead(tmmbr, measured_overhead) \
	((tmmbr)->value) = htonl((ntohl((tmmbr)->value) & 0xFFFFFE00) | ((measured_overhead) & 0x000001FF))

typedef struct rtcp_fb_fir_fci {
	uint32_t ssrc;
	uint8_t seq_nr;
	uint8_t pad1;
	uint16_t pad2;
} rtcp_fb_fir_fci_t;

#define rtcp_fb_fir_fci_get_ssrc(fci) ntohl((fci)->ssrc)
#define rtcp_fb_fir_fci_get_seq_nr(fci) (fci)->seq_nr

typedef struct rtcp_fb_sli_fci {
	uint32_t value;
} rtcp_fb_sli_fci_t;

#define rtcp_fb_sli_fci_get_first(fci) \
	((uint16_t)((ntohl((fci)->value) >> 19) & 0x00001FFF))
#define rtcp_fb_sli_fci_set_first(fci, first) \
	((fci)->value) = htonl((ntohl((fci)->value) & 0x0007FFFF) | (((first) & 0x00001FFF) << 19))
#define rtcp_fb_sli_fci_get_number(fci) \
	((uint16_t)((ntohl((fci)->value) >> 6) & 0x00001FFF))
#define rtcp_fb_sli_fci_set_number(fci, number) \
	((fci)->value) = htonl((ntohl((fci)->value) & 0xFFF8003F) | (((number) & 0x00001FFF) << 6))
#define rtcp_fb_sli_fci_get_picture_id(fci) \
	((uint8_t)(ntohl((fci)->value) & 0x0000003F))
#define rtcp_fb_sli_fci_set_picture_id(fci, picture_id) \
	((fci)->value) = htonl((ntohl((fci)->value) & 0xFFFFFFC0) | ((picture_id) & 0x0000003F))

typedef struct rtcp_fb_rpsi_fci {
	uint8_t pb;
	uint8_t payload_type;
	uint16_t bit_string[1];
} rtcp_fb_rpsi_fci_t;

#define rtcp_fb_rpsi_fci_get_payload_type(fci) (fci)->payload_type
#define rtcp_fb_rpsi_fci_get_bit_string(fci) ((uint8_t *)(fci)->bit_string)

#define MIN_RTCP_PSFB_PACKET_SIZE (sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t))
#define MIN_RTCP_RTPFB_PACKET_SIZE (sizeof(rtcp_common_header_t) + sizeof(rtcp_fb_header_t))

/* RTCP structs */

typedef struct rtcp_sr{
	rtcp_common_header_t ch;
	uint32_t ssrc;
	sender_info_t si;
	report_block_t rb[1];
} rtcp_sr_t;

typedef struct rtcp_rr{
	rtcp_common_header_t ch;
	uint32_t ssrc;
	report_block_t rb[1];
} rtcp_rr_t;

typedef struct rtcp_app{
	rtcp_common_header_t ch;
	uint32_t ssrc;
	char name[4];
} rtcp_app_t;


#endif  // RTCP_H

