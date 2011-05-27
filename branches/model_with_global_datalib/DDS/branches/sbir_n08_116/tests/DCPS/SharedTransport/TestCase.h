/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_TEST_SHAREDTRANSPORT_H
#define DCPS_TEST_SHAREDTRANSPORT_H

#include "TestFramework_T.h"

class TestCase : public virtual TestPair<TestMessageDataReader,
                                         TestMessageDataWriter> {
public:
  virtual DDS::ReturnCode_t init_transport(
    OpenDDS::DCPS::TransportIdType& transport_id,
    ACE_TString& transport_type);

  virtual int test();
};

#endif  /* DCPS_TEST_SHAREDTRANSPORTORDER_H */
