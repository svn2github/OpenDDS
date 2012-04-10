/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DDS_DCPS_DISCOVERY_H
#define OPENDDS_DDS_DCPS_DISCOVERY_H

#include "dds/DdsDcpsInfoC.h"
#include "RcObject_T.h"
#include "RcHandle_T.h"

#include <string>

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

class ACE_Configuration_Heap;

namespace OpenDDS {
namespace DCPS {

class DomainParticipantImpl;

/**
 * @class Discovery
 *
 * @brief Discovery Strategy interface class
 *
 * This class is an abstract class that acts as an interface for both
 * InfoRepo-based discovery and RTPS Discovery.
 *
 */
class OpenDDS_Dcps_Export Discovery : public RcObject<ACE_SYNCH_MUTEX> {
public:
  /// Key type for storing discovery objects.
  /// Probably should just be Discovery::Key
  typedef std::string RepoKey;

  explicit Discovery(const RepoKey& key) : key_(key) { }

  /// Key value for the default repository IOR.
  static const std::string DEFAULT_REPO;
  static const std::string DEFAULT_RTPS;

  virtual std::string get_stringified_dcps_info_ior();
  virtual DCPSInfo_ptr get_dcps_info() = 0;

  virtual DDS::Subscriber_ptr init_bit(DomainParticipantImpl* participant) = 0;

  virtual RepoId bit_key_to_repo_id(DomainParticipantImpl* participant,
                                    const char* bit_topic_name,
                                    const DDS::BuiltinTopicKey_t& key) const = 0;

  RepoKey key() const { return this->key_; }

  class OpenDDS_Dcps_Export Config {
  public:
    virtual ~Config();
    virtual int discovery_config(ACE_Configuration_Heap& cf) = 0;
  };

protected:
#ifndef DDS_HAS_MINIMUM_BIT
  DDS::ReturnCode_t create_bit_topics(DomainParticipantImpl* participant);
#endif

private:
  RepoKey        key_;

};

typedef RcHandle<Discovery> Discovery_rch;

} // namespace DCPS
} // namespace OpenDDS

#endif /* OPENDDS_DCPS_DISCOVERY_H  */
