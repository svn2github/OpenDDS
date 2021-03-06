/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_TRANSPORTDEFS_H
#define OPENDDS_DCPS_TRANSPORTDEFS_H

#include "dds/DCPS/Definitions.h"
#include "dds/DCPS/Cached_Allocator_With_Overflow_T.h"
#include "dds/DCPS/Definitions.h"
#include "dds/DCPS/debug.h"
#include "ace/Basic_Types.h"
#include "TransportDebug.h"
#include <vector>

class ACE_Message_Block;
class ACE_Data_Block;

/**
 * Guard the allocations for the underlying memory management of the
 * receive processing with the following:
 */
#define RECEIVE_SYNCH ACE_SYNCH_NULL_MUTEX

/// Macro to get the individual configuration value
///  from ACE_Configuration_Heap and cast to the specific
///  type from integer.
#define GET_CONFIG_VALUE(CF, SECT, KEY, VALUE, TYPE)                             \
  {                                                                              \
    ACE_TString stringvalue;                                                     \
    if (CF.get_string_value (SECT, KEY, stringvalue) == -1)                      \
    {                                                                            \
      if (OpenDDS::DCPS::Transport_debug_level > 0)                            \
      {                                                                          \
        ACE_DEBUG ((LM_WARNING,                                                  \
                    ACE_TEXT ("(%P|%t)\"%s\" is not defined in config ")         \
                    ACE_TEXT ("file - using code default.\n"),                   \
                    KEY));                                                       \
      }                                                                          \
    }                                                                            \
    else  if (stringvalue == ACE_TEXT(""))                                       \
    {                                                                            \
      if (OpenDDS::DCPS::Transport_debug_level > 0)                            \
      {                                                                          \
        ACE_DEBUG ((LM_WARNING,                                                  \
                    ACE_TEXT ("(%P|%t)missing VALUE for \"%s\" in config ")      \
                    ACE_TEXT ("file - using code default.\n"),                   \
                    KEY));                                                       \
      }                                                                          \
    }                                                                            \
    else                                                                         \
    {                                                                            \
      VALUE = static_cast<TYPE>(ACE_OS::atoi (stringvalue.c_str ()));            \
    }                                                                            \
  }

/// Macro to get the individual configuration value
///  from ACE_Configuration_Heap as string type.
#define GET_CONFIG_STRING_VALUE(CF, SECT, KEY, VALUE)                            \
  {                                                                              \
    ACE_TString stringvalue;                                                     \
    if (CF.get_string_value (SECT, KEY, stringvalue) == -1)                      \
    {                                                                            \
      if (OpenDDS::DCPS::Transport_debug_level > 0)                              \
      {                                                                          \
        ACE_DEBUG ((LM_WARNING,                                                  \
                    ACE_TEXT ("(%P|%t)\"%s\" is not defined in config ")         \
                    ACE_TEXT ("file - using code default.\n"),                   \
                    KEY));                                                       \
      }                                                                          \
    }                                                                            \
    else  if (stringvalue == ACE_TEXT(""))                                       \
    {                                                                            \
      if (OpenDDS::DCPS::Transport_debug_level > 0)                            \
      {                                                                          \
        ACE_DEBUG ((LM_WARNING,                                                  \
                    ACE_TEXT ("(%P|%t)missing VALUE for \"%s\" in config ")      \
                    ACE_TEXT ("file - using code default.\n"),                   \
                    KEY));                                                       \
      }                                                                          \
    }                                                                            \
    else                                                                         \
    {                                                                            \
      VALUE = stringvalue;                                                       \
    }                                                                            \
  }

#define GET_CONFIG_DOUBLE_VALUE(CF, SECT, KEY, VALUE)                            \
  {                                                                              \
    ACE_TString stringvalue;                                                     \
    if (CF.get_string_value (SECT, KEY, stringvalue) == -1)                      \
    {                                                                            \
      if (OpenDDS::DCPS::Transport_debug_level > 0)                            \
      {                                                                          \
        ACE_DEBUG ((LM_WARNING,                                                  \
                    ACE_TEXT ("(%P|%t)\"%s\" is not defined in config ")         \
                    ACE_TEXT ("file - using code default.\n"),                   \
                    KEY));                                                       \
      }                                                                          \
    }                                                                            \
    else  if (stringvalue == ACE_TEXT(""))                                       \
    {                                                                            \
      if (OpenDDS::DCPS::Transport_debug_level > 0)                            \
      {                                                                          \
        ACE_DEBUG ((LM_WARNING,                                                  \
                    ACE_TEXT ("(%P|%t)missing VALUE for \"%s\" in config ")      \
                    ACE_TEXT ("file - using code default.\n"),                   \
                    KEY));                                                       \
      }                                                                          \
    }                                                                            \
    else                                                                         \
    {                                                                            \
      VALUE = ACE_OS::strtod (stringvalue.c_str (), 0);                          \
    }                                                                            \
  }

/// Macro to get the individual configuration value
///  from ACE_Configuration_Heap as ACE_Time_Value
///  using milliseconds.
#define GET_CONFIG_TIME_VALUE(CF, SECT, KEY, VALUE)                              \
  {                                                                              \
    long tv = -1;                                                                \
    GET_CONFIG_VALUE(CF, SECT, KEY, tv, long);                                   \
    if (tv != -1) VALUE.msec(tv);                                                \
  }

// The transport section name prefix.
static const ACE_TCHAR  TRANSPORT_SECTION_NAME_PREFIX[] =
  ACE_TEXT("transport_impl_");
// The transport section name prefix is "transport_impl_" so the length is 15.
static const size_t TRANSPORT_SECTION_NAME_PREFIX_LEN =
  ACE_OS::strlen(TRANSPORT_SECTION_NAME_PREFIX);

namespace OpenDDS {
namespace DCPS {

// Values used in TransportFactory::create_transport_impl () call.
// ciju: Doesn't add any value. Removing.
const bool AUTO_CONFIG = 1;
const bool DONT_AUTO_CONFIG = 0;

/// The TransportImplFactory instance ID type.
typedef ACE_TString FactoryIdType;

/// The TransportImpl instance ID type.
typedef ACE_UINT32 TransportIdType;
typedef std::vector <TransportIdType> TransportIdList;

/// Identifier type for DataLink objects.
typedef ACE_UINT64  DataLinkIdType;

// Note: The range 0xFFFFFF00 to 0xFFFFFFFF is reserved for transport
//       DEFAULT_<transport>_ID values. If a new transport is
//       implemented, the default ID of the new transport must be
//       defined here.
const TransportIdType DEFAULT_SIMPLE_TCP_ID = 0xFFFFFF00;
const TransportIdType DEFAULT_DUMMY_TCP_ID = 0xFFFFFF01;

const TransportIdType DEFAULT_UDP_ID = 0xFFFFFF04;

// The default multicast ID forces the group address selection
// heuristic to resolve port number 49152; this is the minimal
// port defined in the dynamic/private range [IANA 2009-11-16].
const TransportIdType DEFAULT_MULTICAST_ID = 0xFFFFFF08;

/// Return code type for send_control() operations.
enum SendControlStatus {
  SEND_CONTROL_ERROR,
  SEND_CONTROL_OK
};

/// Return code type for attach_transport() operations.
enum AttachStatus {
  ATTACH_BAD_TRANSPORT,
  ATTACH_ERROR,
  ATTACH_INCOMPATIBLE_QOS,
  ATTACH_OK
};

// TBD - Resolve this
#if 0
// TBD SOON - The MAX_SEND_BLOCKS may conflict with the
//            max_samples_per_packet_ configuration!
//MJM: The constant should just be used as part of a conditional at the
//MJM: points where it is used.
#endif

/// Controls the maximum size of the iovec array used for a send packet.
#if defined (ACE_IOV_MAX) && (ACE_IOV_MAX != 0)
enum { MAX_SEND_BLOCKS = ACE_IOV_MAX };
#else
enum { MAX_SEND_BLOCKS = 50 };
#endif

// Allocators are locked since we can not restrict the thread on
// which a release will occur.

/// Allocators used for transport receiving logic.
enum { RECEIVE_DATA_BUFFER_SIZE = 65536 } ;

typedef Cached_Allocator_With_Overflow<ACE_Message_Block, RECEIVE_SYNCH>
TransportMessageBlockAllocator ;

typedef Cached_Allocator_With_Overflow<ACE_Data_Block,    RECEIVE_SYNCH>
TransportDataBlockAllocator ;

typedef Cached_Allocator_With_Overflow<
char[RECEIVE_DATA_BUFFER_SIZE],
RECEIVE_SYNCH>                  TransportDataAllocator ;

/// Default TransportConfiguration settings
enum {
  DEFAULT_CONFIG_QUEUE_MESSAGES_PER_POOL   = 10,
  DEFAULT_CONFIG_QUEUE_INITIAL_POOLS    = 5,
  DEFAULT_CONFIG_MAX_PACKET_SIZE        = 2147481599,
  DEFAULT_CONFIG_MAX_SAMPLES_PER_PACKET = 10,
  DEFAULT_CONFIG_OPTIMUM_PACKET_SIZE    = 4096
};

} // namespace DCPS
} // namespace OpenDDS

#endif  /* OPENDDS_DCPS_TRANSPORTDEFS_H */
