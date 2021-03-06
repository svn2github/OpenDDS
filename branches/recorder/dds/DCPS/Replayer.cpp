/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */
#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/
#include "Replayer.h"

namespace OpenDDS {
namespace DCPS {
ReplayerListener::~ReplayerListener()
{
}

void ReplayerListener::on_replayer_matched(Replayer*                               replayer,
                                           const ::DDS::PublicationMatchedStatus & status )
{
}

Replayer::~Replayer()
{
}

} // namespace DCPS
} // namespace
