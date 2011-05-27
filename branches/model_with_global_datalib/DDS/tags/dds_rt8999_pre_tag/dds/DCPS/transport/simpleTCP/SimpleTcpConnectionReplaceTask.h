// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_SIMPLETCPCONNECTIONREPLACETASK_H
#define TAO_DCPS_SIMPLETCPCONNECTIONREPLACETASK_H

#include /**/ "ace/pre.h"

#include  "SimpleTcpConnection_rch.h"
#include  "dds/DCPS/transport/framework/QueueTaskBase_T.h"


#if !defined (ACE_LACKS_PRAGMA_ONCE)
# pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */


namespace TAO
{

  namespace DCPS
  {
    class SimpleTcpTransport;

    /**
     * @class SimpleTcpConnectionReplaceTask
     *
     * @brief Active Object managing a queue of connection info objects.
     *
     *  This task is dedicated to check if the incoming connections are re-established
     *  connection from the remote. This would resolve the deadlock problem between the
     *  reactor thread (calling SimpleTcpTransport::passive_connction()) and the orb
     *  thread (calling SimpleTcpTransport::make_passive_connction()). The reactor
     *  thread will enqueue the new connection to this task and let this task dequeue
     *  and check the connection. This task handles all connections associated with 
     *  a TransportImpl object.
     */
    class SimpleTcpConnectionReplaceTask : public QueueTaskBase <SimpleTcpConnection_rch>
    {
    public:
   
      

      /// Constructor.
      SimpleTcpConnectionReplaceTask(SimpleTcpTransport* trans);

      /// Virtual Destructor.
      virtual ~SimpleTcpConnectionReplaceTask();

      /// Handle the request.
      virtual void execute (SimpleTcpConnection_rch& con);

    private:

      SimpleTcpTransport* trans_;
    };
  }
}

#include /**/ "ace/post.h"

#endif /* TAO_DCPS_SIMPLETCPCONNECTIONREPLACETASK_H */
