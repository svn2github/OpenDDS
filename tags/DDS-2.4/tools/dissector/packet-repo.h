// -*- C++ -*-
/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */


#ifndef _PACKET_REPO_H_
#define _PACKET_REPO_H_

extern "C" {

#include "config.h"

#include <glib.h>
#include <gmodule.h>

#include <epan/packet.h>
#include <epan/dissectors/packet-giop.h>
} // extern "C"

#include "dds/DCPS/Definitions.h"
#include "tools/dissector/dissector_export.h"
#include "tools/dissector/giop_base.h"

#include "ace/Synch.h"
#include "ace/Hash_Map_Manager.h"

namespace OpenDDS
{
  namespace DCPS
  {

    struct Pending
    {
      conversation_t *conv_;
      gulong          request_;
      char           *data_name_; // for pending topics
      RepoId         *topic_id_; // for pending publications
      Pending  *next_;

      Pending ()
        :conv_(0), request_(0), data_name_(0), topic_id_(0), next_(0) {}
      ~Pending ()
      {
        delete [] data_name_;
        delete topic_id_;
      }

    };

    typedef ACE_Hash_Map_Manager <gulong, const char *, ACE_Null_Mutex> Known_Topics;
    typedef ACE_Hash_Map_Manager <gulong, const RepoId *, ACE_Null_Mutex> Known_Publications;

    class dissector_Export InfoRepo_Dissector : public GIOP_Base
    {
    public:
      const char *topic_for_pub (const RepoId *);

      static InfoRepo_Dissector& instance ();

      static gboolean explicit_giop_callback (tvbuff_t *, packet_info *,
                                              proto_tree *,int *,
                                              ::MessageHeader *, gchar *,
                                              gchar *);
      static gboolean heuristic_giop_callback (tvbuff_t *, packet_info *,
                                              proto_tree *,int *,
                                              ::MessageHeader *, gchar *,
                                              gchar *);

      virtual void init ();
      void register_handoff ();

      static void assert_topic            (::MessageHeader *);
      static void add_publication         (::MessageHeader *);
      static void add_domain_participant  (::MessageHeader *);
      static void add_subscription        (::MessageHeader *);
      static void remove_publication      (::MessageHeader *);
      static void remove_subscription     (::MessageHeader *);
      static void remove_domain_participant (::MessageHeader *);

#if 0
      static void attach_participant             (::MessageHeader *);
      static void find_topic                     (::MessageHeader *);
      static void remove_topic                   (::MessageHeader *);
      static void enable_topic                   (::MessageHeader *);
      static void remove_domain_participant      (::MessageHeader *);
      static void disassociate_participant       (::MessageHeader *);
      static void disassociate_subscription      (::MessageHeader *);
      static void disassociate_publication       (::MessageHeader *);
      static void ignore_domain_participant      (::MessageHeader *);
      static void ignore_topic                   (::MessageHeader *);
      static void ignore_subscription            (::MessageHeader *);
      static void ignore_publication             (::MessageHeader *);
      static void update_domain_participant_qos  (::MessageHeader *);
      static void update_topic_qos               (::MessageHeader *);
      static void update_publication_qos         (::MessageHeader *);
      static void update_subscription_qos        (::MessageHeader *);
      static void update_subscription_params     (::MessageHeader *);
      static void shutdown                       (::MessageHeader *);
#endif
    private:
      void add_pending (int request_id, const char *dataName);
      void add_pending (int request_id, const RepoId *topic_id);
      void map_pending (Pending &, const RepoId *);
      void discard_pending (Pending &);

      Pending           *pending_;
      Known_Topics       topics_;
      Known_Publications publications_;

      static InfoRepo_Dissector instance_;

      static bool initialized_;
      //----
      // these are "header field" indicies
      static int hf_topicId;
      static int hf_topicStatus;

      static int hf_domainId;
      static int hf_participantId;
      static int hf_topicName;
      static int hf_dataTypeName;
      static int hf_topicQos;

      static int hf_addEntityRetn;
      static int hf_exception;
      static int hf_qos;
      static int hf_transInfo;

      //-----
      // these are sub-tree indicies
      static int ett_ior_;
      static int ett_topic_qos_;
      static int ett_domain_qos_;
      static int ett_writer_qos_;
      static int ett_publisher_qos_;
      static int ett_trans_info_;
    };

  } // namespace DCPS
} // namespace OpenDDS


#endif //  _PACKET_REPO_H_
