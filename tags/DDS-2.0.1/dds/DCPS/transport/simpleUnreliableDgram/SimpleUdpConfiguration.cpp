/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "SimpleUnreliableDgram_pch.h"
#include "SimpleUdpConfiguration.h"

#if !defined (__ACE_INLINE__)
#include "SimpleUdpConfiguration.inl"
#endif /* __ACE_INLINE__ */

OpenDDS::DCPS::SimpleUdpConfiguration::~SimpleUdpConfiguration()
{
  DBG_ENTRY_LVL("SimpleUdpConfiguration","~SimpleUdpConfiguration",6);
}

int
OpenDDS::DCPS::SimpleUdpConfiguration::load(const TransportIdType& id,
                                            ACE_Configuration_Heap& cf)
{
  // The default transport can not be configured by user.
  if (id == DEFAULT_SIMPLE_UDP_ID) {
    ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t)You can not configure the default SimpleUdp transport(id=%u) !!!\n"),
               id));
    return -1;
  }

  // Call the base class method through 'this' to help VC6 figure out
  // what to do.
  this->TransportConfiguration::load(id, cf);

  ACE_TString sect_name = id_to_section_name(id);
  const ACE_Configuration_Section_Key &root = cf.root_section();
  ACE_Configuration_Section_Key trans_sect;

  if (cf.open_section(root, sect_name.c_str(), 0, trans_sect) != 0)
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("Failed to open section: %s\n"),
                      sect_name.c_str()), -1);

  ACE_TString local_address;
  GET_CONFIG_STRING_VALUE(cf, trans_sect, ACE_TEXT("local_address"), local_address);

  if (local_address != ACE_TEXT("")) {
    this->local_address_str_ = local_address;
    this->local_address_.set(local_address_str_.c_str());
  }

  GET_CONFIG_VALUE(cf, trans_sect, ACE_TEXT("max_output_pause_period"),
                   this->max_output_pause_period_, int);

  return 0;
}
