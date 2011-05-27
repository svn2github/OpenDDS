/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

extern "C" {

#include "config.h"

#include <glib.h>
#include <gmodule.h>

#include <epan/ipproto.h>
#include <epan/packet.h>

} // extern "C"

#include <ace/Basic_Types.h>
#include <ace/CDR_Base.h>
#include <ace/Message_Block.h>

#include <dds/DCPS/DataSampleHeader.h>
#include <dds/DCPS/RepoIdConverter.h>
#include <dds/DdsDcpsGuidTypeSupportImpl.h>
#include <dds/DCPS/transport/framework/TransportHeader.h>

#include <cstring>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "odds_export.h"

using namespace OpenDDS::DCPS;

namespace {

int proto_odds    = -1;

int hf_version    = -1;
int hf_byte_order = -1;
int hf_length     = -1;
int hf_sequence   = -1;
int hf_source     = -1;

int hf_sample             = -1;
int hf_sample_id          = -1;
int hf_sample_sub_id      = -1;
int hf_sample_length      = -1;
int hf_sample_sequence    = -1;
int hf_sample_timestamp   = -1;
int hf_sample_lifespan    = -1;
int hf_sample_publication = -1;

int hf_sample_flags             = -1;
int hf_sample_flags_byte_order  = -1;
int hf_sample_flags_coherent    = -1;
int hf_sample_flags_historic    = -1;
int hf_sample_flags_lifespan    = -1;

const int sample_flags_bits = 8;

const int* sample_flags_fields[] = {
  &hf_sample_flags_byte_order,
  &hf_sample_flags_coherent,
  &hf_sample_flags_historic,
  &hf_sample_flags_lifespan,
  NULL
};

gint ett_header       = -1;
gint ett_sample       = -1;
gint ett_sample_flags = -1;

const value_string byte_order_vals[] = {
  { 0x0,  "Big Endian"    },
  { 0x1,  "Little Endian" },
  { 0,    NULL            }
};

const true_false_string byte_order_tfs = {
  "Little Endian",
  "Big Endian"
};

const value_string sample_id_vals[] = {
  { SAMPLE_DATA,            "SAMPLE_DATA"           },
  { DATAWRITER_LIVELINESS,  "DATAWRITER_LIVELINESS" },
  { INSTANCE_REGISTRATION,  "INSTANCE_REGISTRATION" },
  { UNREGISTER_INSTANCE,    "UNREGISTER_INSTANCE"   },
  { DISPOSE_INSTANCE,       "DISPOSE_INSTANCE"      },
  { GRACEFUL_DISCONNECT,    "GRACEFUL_DISCONNECT"   },
  { FULLY_ASSOCIATED,       "FULLY_ASSOCIATED"      },
  { REQUEST_ACK,            "REQUEST_ACK"           },
  { SAMPLE_ACK,             "SAMPLE_ACK"            },
  { END_COHERENT_CHANGES,   "END_COHERENT_CHANGES"  },
  { TRANSPORT_CONTROL,      "TRANSPORT_CONTROL"     },
  { 0,                      NULL                    }
};

const value_string sample_sub_id_vals[] = {
  { SUBMESSAGE_NONE,        "SUBMESSAGE_NONE"       },
  { MULTICAST_SYN,          "MULTICAST_SYN"         },
  { MULTICAST_SYNACK,       "MULTICAST_SYNACK"      },
  { MULTICAST_NAK,          "MULTICAST_NAK"         },
  { MULTICAST_NAKACK,       "MULTICAST_NAKACK"      },
  { 0,                      NULL                    }
};

template<typename T>
ACE_INLINE T
demarshal_data(tvbuff_t* tvb, gint offset)
{
  T t;

  size_t len = std::min(size_t(tvb->length - offset), t.max_marshaled_size());
  const guint8* data = tvb_get_ptr(tvb, offset, len);

  ACE_Message_Block mb(reinterpret_cast<const char*>(data));
  mb.wr_ptr(len);

  t.init(&mb);

  return t;
}

std::string
format_header(const TransportHeader& header)
{
  std::ostringstream os;

  os << "Length: " << std::dec << header.length_;
  os << ", Sequence: 0x" << std::hex << std::setw(4) << std::setfill('0')
     << ACE_UINT16(header.sequence_);
  os << ", Source: 0x" << std::hex << std::setw(8) << std::setfill('0')
     << ACE_UINT32(header.source_);

  return os.str();
}

void
dissect_header(tvbuff* tvb, packet_info* pinfo, proto_tree* tree,
               const TransportHeader& header, gint& offset)
{
  ACE_UNUSED_ARG(pinfo);

  size_t len;

  offset += sizeof(header.protocol_) - 2; // skip preamble

  // hf_version
  len = sizeof(header.protocol_) - offset;
  proto_tree_add_bytes_format_value(tree, hf_version, tvb, offset, len,
    reinterpret_cast<const guint8*>(header.protocol_ + offset),
    "%d.%d", header.protocol_[4], header.protocol_[5]);
  offset += len;

  // hf_byte_order
  len = sizeof(header.byte_order_);
  proto_tree_add_item(tree, hf_byte_order, tvb, offset, len, FALSE);
  offset += len;

  offset += sizeof(header.reserved_);     // skip reserved

  // hf_length
  len = sizeof(header.length_);
  proto_tree_add_uint_format_value(tree, hf_length, tvb, offset, len,
    header.length_, "%d octets", header.length_);
  offset += len;

  // hf_sequence
  len = sizeof(header.sequence_);
  proto_tree_add_uint(tree, hf_sequence, tvb, offset, len,
    guint16(header.sequence_));
  offset += len;

  // hf_source
  len = sizeof(header.source_);
  proto_tree_add_uint(tree, hf_source, tvb, offset, len,
    guint32(header.source_));
  offset += len;
}

std::string
format_sample(const DataSampleHeader& sample)
{
  std::ostringstream os;

  if (sample.submessage_id_ != SUBMESSAGE_NONE) {
    os << SubMessageId(sample.submessage_id_)
       << " (0x" << std::hex << std::setw(2) << std::setfill('0')
       << unsigned(sample.submessage_id_) << ")";
  } else {
    os << MessageId(sample.message_id_)
       << " (0x" << std::hex << std::setw(2) << std::setfill('0')
       << unsigned(sample.message_id_) << ")";
  }

  os << ", Length: " << std::dec << sample.message_length_;

  if (sample.message_id_ != TRANSPORT_CONTROL) {
    if (sample.message_id_ == SAMPLE_DATA) {
      os << ", Sequence: 0x" << std::hex << std::setw(4) << std::setfill('0')
         << ACE_UINT16(sample.sequence_);
    }
    os << ", Publication: " << RepoIdConverter(sample.publication_id_);
  }

  return os.str();
}

void
dissect_sample(tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree,
               const DataSampleHeader& sample, gint& offset)
{
  ACE_UNUSED_ARG(pinfo);

  size_t len;

  // hf_sample_id
  len = sizeof(sample.message_id_);
  proto_tree_add_item(tree, hf_sample_id, tvb, offset, len, FALSE);
  offset += len;

  // hf_sample_sub_id
  len = sizeof(sample.submessage_id_);
  if (sample.submessage_id_ != SUBMESSAGE_NONE) {
    proto_tree_add_item(tree, hf_sample_sub_id, tvb, offset, len, FALSE);
  }
  offset += len;

  // hf_sample_flags
  len = sizeof(ACE_CDR::Octet);
  proto_tree_add_bitmask(tree, tvb, offset,
    hf_sample_flags, ett_sample_flags, sample_flags_fields, FALSE);
  offset += len;

  // hf_sample_length
  len = sizeof(sample.message_length_);
  proto_tree_add_uint_format_value(tree, hf_sample_length, tvb, offset, len,
    sample.message_length_, "%d octets", sample.message_length_);
  offset += len;

  // hf_sample_sequence
  len = sizeof(sample.sequence_);
  if (sample.message_id_ == SAMPLE_DATA) {
    proto_tree_add_uint(tree, hf_sample_sequence, tvb, offset, len,
      guint16(sample.sequence_));
  }
  offset += len;

  // hf_sample_timestamp
  len = sizeof(sample.source_timestamp_sec_) +
        sizeof(sample.source_timestamp_nanosec_);
  if (sample.message_id_ != TRANSPORT_CONTROL) {
    nstime_t ns = {
      sample.source_timestamp_sec_,
      sample.source_timestamp_nanosec_
    };
    proto_tree_add_time(tree, hf_sample_timestamp, tvb, offset, len, &ns);
  }
  offset += len;

  // hf_sample_lifespan
  if (sample.lifespan_duration_) {
    len = sizeof(sample.lifespan_duration_sec_) +
          sizeof(sample.lifespan_duration_nanosec_);
    if (sample.message_id_ != TRANSPORT_CONTROL) {
      nstime_t ns = {
        sample.lifespan_duration_sec_,
        sample.lifespan_duration_nanosec_
      };
      proto_tree_add_time(tree, hf_sample_lifespan, tvb, offset, len, &ns);
    }
    offset += len;
  }

  // hf_sample_publication
  len = gen_find_size(sample.publication_id_);
  if (sample.message_id_ != TRANSPORT_CONTROL) {
    RepoIdConverter converter(sample.publication_id_);
    proto_tree_add_bytes_format_value(tree, hf_sample_publication, tvb, offset, len,
      reinterpret_cast<const guint8*>(&sample.publication_id_),
      std::string(converter).c_str());
  }
  offset += len;

  offset += sample.message_length_; // skip marshaled data
}

} // namespace

extern "C"
odds_Export void
dissect_odds(tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree)
{
  gint offset = 0;

  if (check_col(pinfo->cinfo, COL_PROTOCOL)) {
    col_set_str(pinfo->cinfo, COL_PROTOCOL, "OpenDDS");
  }

  if (check_col(pinfo->cinfo, COL_INFO)) {
    col_clear(pinfo->cinfo, COL_INFO);
  }

  TransportHeader header = demarshal_data<TransportHeader>(tvb, offset);
  std::string header_str(format_header(header));

  if (check_col(pinfo->cinfo, COL_INFO)) {
    col_add_fstr(pinfo->cinfo, COL_INFO, "DCPS %s", header_str.c_str());
  }

  if (tree != NULL) {
    proto_item* item =
      proto_tree_add_protocol_format(tree, proto_odds, tvb, 0, -1,
        "OpenDDS DCPS Protocol, %s", header_str.c_str());

    proto_tree* header_tree = proto_item_add_subtree(item, ett_header);

    dissect_header(tvb, pinfo, header_tree, header, offset);

    while (offset < gint(tvb->length)) {
      DataSampleHeader sample = demarshal_data<DataSampleHeader>(tvb, offset);
      std::string sample_str(format_sample(sample));

      proto_item* item =
        proto_tree_add_none_format(header_tree, hf_sample, tvb, offset,
          sample.marshaled_size() + sample.message_length_, sample_str.c_str());

      proto_tree* sample_tree = proto_item_add_subtree(item, ett_sample);

      dissect_sample(tvb, pinfo, sample_tree, sample, offset);
    }
  }
}

extern "C"
odds_Export gboolean
dissect_odds_heur(tvbuff_t* tvb, packet_info* pinfo, proto_tree* tree)
{
  size_t len = sizeof(TransportHeader::DCPS_PROTOCOL);
  guint8* data = tvb_get_ephemeral_string(tvb, 0, len);

  if (std::memcmp(data, TransportHeader::DCPS_PROTOCOL, len) != 0) {
    return FALSE;
  }

  dissect_odds(tvb, pinfo, tree);
  return TRUE;
}

extern "C"
odds_Export void
proto_register_odds()
{
  static hf_register_info hf[] = {
    { &hf_version,
      { "Version",
        "odds.version",
        FT_BYTES,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_byte_order,
      { "Byte order",
        "odds.byte_order",
        FT_UINT8,
        BASE_HEX,
        VALS(byte_order_vals),
        0,
        NULL,
        HFILL
      }
    },
    { &hf_length,
      { "Length",
        "odds.length",
        FT_UINT16,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sequence,
      { "Sequence",
        "odds.sequence",
        FT_UINT16,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_source,
      { "Source",
        "odds.source",
        FT_UINT32,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample,
      { "Sample",
        "odds.sample",
        FT_NONE,
        BASE_NONE,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_id,
      { "ID",
        "odds.sample.id",
        FT_UINT8,
        BASE_HEX,
        VALS(sample_id_vals),
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_sub_id,
      { "Sub-ID",
        "odds.sample.sub_id",
        FT_UINT8,
        BASE_HEX,
        VALS(sample_sub_id_vals),
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_flags,
      { "Flags",
        "odds.sample.flags",
        FT_UINT8,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_flags_byte_order,
      { "Byte order",
        "odds.sample.flags.byte_order",
        FT_BOOLEAN,
        sample_flags_bits,
        TFS(&byte_order_tfs),
        1 << 0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_flags_coherent,
      { "Coherent",
        "odds.sample.flags.coherent",
        FT_BOOLEAN,
        sample_flags_bits,
        NULL,
        1 << 1,
        NULL,
        HFILL
      }
    },
    { &hf_sample_flags_historic,
      { "Historic",
        "odds.sample.flags.historic",
        FT_BOOLEAN,
        sample_flags_bits,
        NULL,
        1 << 2,
        NULL,
        HFILL
      }
    },
    { &hf_sample_flags_lifespan,
      { "Lifespan",
        "odds.sample.flags.lifespan",
        FT_BOOLEAN,
        sample_flags_bits,
        NULL,
        1 << 3,
        NULL,
        HFILL
      }
    },
    { &hf_sample_length,
      { "Length",
        "odds.sample.length",
        FT_UINT32,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_sequence,
      { "Sequence",
        "odds.sample.sequence",
        FT_UINT16,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_timestamp,
      { "Timestamp",
        "odds.sample.timestamp",
        FT_ABSOLUTE_TIME,
        BASE_NONE,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_lifespan,
      { "Lifespan",
        "odds.sample.lifespan",
        FT_RELATIVE_TIME,
        BASE_NONE,
        NULL,
        0,
        NULL,
        HFILL
      }
    },
    { &hf_sample_publication,
      { "Publication",
        "odds.sample.publication",
        FT_BYTES,
        BASE_HEX,
        NULL,
        0,
        NULL,
        HFILL
      }
    }
  };

  static gint *ett[] = {
    &ett_header,
    &ett_sample,
    &ett_sample_flags
  };

  proto_odds = proto_register_protocol(
    "OpenDDS DCPS Protocol",  // name
    "OpenDDS",                // short_name
    "odds");                  // filter_name

  proto_register_field_array(proto_odds, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));
}

extern "C"
odds_Export void
proto_reg_handoff_odds()
{
  static dissector_handle_t odds_handle =
    create_dissector_handle(dissect_odds, proto_odds);

  ACE_UNUSED_ARG(odds_handle);

  heur_dissector_add("tcp", dissect_odds_heur, proto_odds);
  heur_dissector_add("udp", dissect_odds_heur, proto_odds);
}
