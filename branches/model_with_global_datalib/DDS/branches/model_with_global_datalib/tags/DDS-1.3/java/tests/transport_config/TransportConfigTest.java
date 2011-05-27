import org.omg.CORBA.StringSeqHolder;
import org.omg.CORBA.SystemException;

import OpenDDS.DCPS.TheServiceParticipant;
import OpenDDS.DCPS.TheParticipantFactory;

import OpenDDS.DCPS.transport.TheTransportFactory;
import OpenDDS.DCPS.transport.TransportImpl;
import OpenDDS.DCPS.transport.TransportException;
import OpenDDS.DCPS.transport.TransportConfiguration;
import OpenDDS.DCPS.transport.SimpleTcpConfiguration;
import OpenDDS.DCPS.transport.SimpleMcastConfiguration;
import OpenDDS.DCPS.transport.SimpleUdpConfiguration;
import OpenDDS.DCPS.transport.ReliableMulticastConfiguration;

public class TransportConfigTest {

    protected static void setUp(String[] args) {

        // For debugging:
        //    OpenDDS.DCPS.TheParticipantFactory.loadNativeLib();
        //    System.out.println("READY");
        //    try {System.in.read();} catch (java.io.IOException ioe) {}

        // We need to load the SimpleTcp lib before initializing the
        // ParticipantFactory, because it will read and parse the config file.
        TheTransportFactory.get_or_create_configuration(99 /*bogus ID*/,
            TheTransportFactory.TRANSPORT_TCP);
        TheParticipantFactory.WithArgs(new StringSeqHolder(args));
    }


    public static void main(String[] args) throws Exception {
        setUp(args);
        try {
            testModifyTransportFromFileTCP();
            testCreateNewTransportRMCast();
            testCreateNewTransportUDP();
            testCreateNewTransportMCast();
        } catch (SystemException se) {
            se.printStackTrace();
            assert false;
        } catch (TransportException te) {
            te.printStackTrace();
            assert false;
        } finally {
            tearDown();
        }
    }


    protected static void testModifyTransportFromFileTCP() throws Exception {
        final int ID = 1; //matches .ini file
        TransportConfiguration tc =
            TheTransportFactory.get_or_create_configuration(ID,
                TheTransportFactory.TRANSPORT_TCP);
        // Verify the values were read in from the ini file
        assert tc.isSwapBytes();
        assert tc.getMaxSamplesPerPacket() == 5;
        SimpleTcpConfiguration tc_tcp = (SimpleTcpConfiguration) tc;
        assert tc_tcp.getConnRetryAttempts() == 42;

        tc.setSwapBytes(false);

        TransportImpl ti = TheTransportFactory.create_transport_impl(ID,
            TheTransportFactory.TRANSPORT_TCP,
            TheTransportFactory.DONT_AUTO_CONFIG);
        ti.configure(tc);

        tc = TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_TCP);
        assert !tc.isSwapBytes();
        assert tc.getMaxSamplesPerPacket() == 5;

        TheTransportFactory.release(ID);
    }


    protected static void testCreateNewTransportRMCast() throws Exception {
        final int ID = 2;
        TransportConfiguration tc =
            TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_RMCAST);
        ReliableMulticastConfiguration rmc =
            (ReliableMulticastConfiguration) tc;
        rmc.setLocalAddress("0.0.0.0:2048");
        rmc.setMulticastGroupAddress("224.0.0.1:29823");
        rmc.setReceiver(true);
        rmc.setSenderHistorySize(12);
        rmc.setReceiverBufferSize(13);

        TransportImpl ti = TheTransportFactory.create_transport_impl(ID,
            TheTransportFactory.TRANSPORT_RMCAST,
            TheTransportFactory.DONT_AUTO_CONFIG);
        ti.configure(tc);

        tc = TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_RMCAST);
        rmc = (ReliableMulticastConfiguration) tc;
        assert rmc.getLocalAddress().equals("0.0.0.0:2048");
        assert rmc.getMulticastGroupAddress().equals("224.0.0.1:29823");
        assert rmc.isReceiver();
        assert rmc.getSenderHistorySize() == 12;
        assert rmc.getReceiverBufferSize() == 13;

        TheTransportFactory.release(ID);
    }


    protected static void testCreateNewTransportUDP() throws Exception {
        final int ID = 3;
        TransportConfiguration tc =
            TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_UDP_UNI);
        tc.setSendThreadStrategy(TransportConfiguration.ThreadSynchStrategy.NULL_SYNCH);
        SimpleUdpConfiguration suc = (SimpleUdpConfiguration) tc;
        suc.setLocalAddress("0.0.0.0:2047");
        suc.setMaxOutputPausePeriod(123456);

        TransportImpl ti = TheTransportFactory.create_transport_impl(ID,
            TheTransportFactory.TRANSPORT_UDP_UNI,
            TheTransportFactory.DONT_AUTO_CONFIG);
        ti.configure(tc);

        tc = TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_UDP_UNI);
        assert tc.getSendThreadStrategy() == TransportConfiguration.ThreadSynchStrategy.NULL_SYNCH;
        suc = (SimpleUdpConfiguration) tc;
        //Only checking endsWith here b/c the 0.0.0.0 is resolved to a hostname
        assert suc.getLocalAddress().endsWith(":2047");
        assert suc.getMaxOutputPausePeriod() == 123456;

        TheTransportFactory.release(ID);
    }


    protected static void testCreateNewTransportMCast() throws Exception {
        final int ID = 4;
        TransportConfiguration tc =
            TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_UDP_MULTI);
        SimpleMcastConfiguration smc = (SimpleMcastConfiguration) tc;
        smc.setMulticastGroupAddress("224.0.0.1:29824");
        smc.setReceiver(false);

        TransportImpl ti = TheTransportFactory.create_transport_impl(ID,
            TheTransportFactory.TRANSPORT_UDP_MULTI,
            TheTransportFactory.DONT_AUTO_CONFIG);
        ti.configure(tc);

        tc = TheTransportFactory.get_or_create_configuration(ID,
                 TheTransportFactory.TRANSPORT_UDP_MULTI);
        smc = (SimpleMcastConfiguration) tc;
        assert smc.getMulticastGroupAddress().equals("224.0.0.1:29824");
        assert !smc.isReceiver();

        TheTransportFactory.release(ID);
    }


    protected static void tearDown() {
        TheTransportFactory.release();
        TheServiceParticipant.shutdown();
    }
}
