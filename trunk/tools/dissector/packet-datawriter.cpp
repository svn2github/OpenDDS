/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "tools/dissector/packet-datawriter.h"
#include "dds/DdsDcpsInfoUtilsC.h"
#include "dds/DCPS/RepoIdConverter.h"

#include "ace/Basic_Types.h"
#include "ace/CDR_Base.h"
#include "ace/Message_Block.h"
#include "ace/Log_Msg.h"
#include "ace/OS_NS_string.h"
#include "ace/ACE.h"

#include <cstring>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

// value ABSOLUTE_TIME_LOCAL, 1.2.x does not.  This technique uses
// the ABSOLUTE_TIME_LOCAL value if it is present (1.3.x),
// and uses BASE_NONE if it is not (1.2.x).  This must be in
// the same scope as the Wireshark 1.2.x declaration of
// the ABSOLUTE_TIME_LOCAL enum value, which is why it is in the
// global namespace.
struct ABSOLUTE_TIME_LOCAL {
  static const int value = BASE_NONE;
};

namespace {

  // These two functions are the rest of the
  // Wireshark 1.2.x / 1.3.x compatibility solution.
  template <int V> int enum_value() { return V; }
  template <typename T> int enum_value() { return T::value; }

  const value_string byte_order_vals[] = {
    { 0x0,  "Big Endian"    },
    { 0x1,  "Little Endian" },
    { 0,    NULL            }
  };

  const true_false_string byte_order_tfs = {
    "Little Endian",
    "Big Endian"
  };

  const value_string topic_status_vals[] = {
    { OpenDDS::DCPS::CREATED, "CREATED" },
    { OpenDDS::DCPS::ENABLED, "ENABLED" },
    { OpenDDS::DCPS::FOUND, "FOUND" },
    { OpenDDS::DCPS::NOT_FOUND, "NOT_FOUND" },
    { OpenDDS::DCPS::REMOVED, "REMOVED" },
    { OpenDDS::DCPS::CONFLICTING_TYPENAME, "CONFLICTING_TYPENAME" },
    { OpenDDS::DCPS::INTERNAL_ERROR, "INTERNAL_ERROR" }
  };

} // namespace


namespace OpenDDS
{
  namespace DCPS
  {
    DataWriterRemote_Dissector DataWriterRemote_Dissector::instance_;

    DataWriterRemote_Dissector&
    DataWriterRemote_Dissector::instance()
    {
      return instance_;
    }

    bool
    DataWriterRemote_Dissector::add_associations (::MessageHeader *header)
    {
      instance().start_decoding ();
      switch (header->message_type) {
      case Reply:
        {
          // parse reply
          //ACE_DEBUG ((LM_DEBUG,"Decoding Add_Associations reply\n"));
          break;
        }
      case Request:
        {
          // parse reply
          // ACE_DEBUG ((LM_DEBUG,"Decoding Add_Associations request\n"));
          break;
        }
      default:
        {
          //ACE_DEBUG ((LM_DEBUG,"Decoding Add_Associations, msg type = %d\n",
          //            header->message_type));
        }
      }

      return true;

    }

    void
    DataWriterRemote_Dissector::init ()
    {
      this->init_proto_label ("DataWriter");
      this->init_repo_id ("OpenDDS/DCPS/DataWriterRemote");
      this->proto_id_ = proto_register_protocol
        ("OpenDDS DataWriterRemote using GIOP",
         this->proto_label_,
         "datawriter-giop");
#if 0
      static gint *ett[] = {
      };

      proto_register_subtree_array(ett, array_length(ett));
#endif

      add_giop_decoder ("add_associations", add_associations);

#if 0
      add_giop_decoder ("remove_associations", remove_associations);
      add_giop_decoder ("update_incompatible_qos", update_incompatible_qos);
      add_giop_decoder ("update_subscription_params", update_subscription_params);
#endif
    }


    extern "C"
    gboolean
    DataWriterRemote_Dissector::explicit_giop_callback
    (tvbuff_t *tvb, packet_info *pinfo,
     proto_tree *ptree, int *offset,
     ::MessageHeader *header, gchar *operation,
     gchar *idlname)
    {
      int ofs = 0;
      header->req_id =
        ::get_CDR_ulong(tvb, &ofs,
                        instance().is_big_endian_, GIOP_HEADER_SIZE);

      if (idlname == 0)
        return FALSE;
      instance().setPacket (tvb, pinfo, ptree, offset);
      if (instance().dissect_giop (header, operation, idlname) == -1)
        return FALSE;
      return TRUE;
    }

    extern "C"
    gboolean
    DataWriterRemote_Dissector::heuristic_giop_callback
    (tvbuff_t *tvb, packet_info *pinfo,
     proto_tree *ptree, int *offset,
     ::MessageHeader *header, gchar *operation, gchar *)
    {
      int ofs = 0;
      header->req_id =
        ::get_CDR_ulong(tvb, &ofs,
                        instance().is_big_endian_, GIOP_HEADER_SIZE);

      instance().setPacket (tvb, pinfo, ptree, offset);

      return instance().dissect_heur (header, operation);
    }

    void
    DataWriterRemote_Dissector::register_handoff ()
    {
      register_giop_user_module(explicit_giop_callback,
                                proto_label_,
                                repo_id_,
                                proto_id_);

      register_giop_user(heuristic_giop_callback,
                         proto_label_, proto_id_);


    }


  }
}
