/*
 * $Id$
 */

package org.opendds.jms.resource;

import java.io.PrintWriter;
import java.util.Set;

import javax.resource.ResourceException;
import javax.resource.spi.ConnectionManager;
import javax.resource.spi.ConnectionRequestInfo;
import javax.resource.spi.ManagedConnection;
import javax.resource.spi.ManagedConnectionFactory;
import javax.security.auth.Subject;

import org.opendds.jms.ConnectionFactoryImpl;
import org.opendds.jms.common.lang.Objects;
import org.opendds.jms.qos.ParticipantQosPolicy;
import org.opendds.jms.qos.PublisherQosPolicy;
import org.opendds.jms.qos.SubscriberQosPolicy;
import org.opendds.jms.transport.TransportConfigurationFactory;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class ManagedConnectionFactoryImpl implements ManagedConnectionFactory {
    private Integer domainId;
    private String participantQosPolicy;
    private String publisherQosPolicy;
    private String publisherTransport;
    private String subscriberQosPolicy;
    private String subscriberTransport;
    private String transportType;

    public Integer getDomainId() {
        return domainId;
    }

    public void setDomainId(Integer domainId) {
        this.domainId = domainId;
    }

    public String getParticipantQosPolicy() {
        return participantQosPolicy;
    }

    public void setParticipantQosPolicy(String participantQosPolicy) {
        this.participantQosPolicy = participantQosPolicy;
    }

    public String getPublisherQosPolicy() {
        return publisherQosPolicy;
    }

    public void setPublisherQosPolicy(String publisherQosPolicy) {
        this.publisherQosPolicy = publisherQosPolicy;
    }

    public String getPublisherTransport() {
        return publisherTransport;
    }

    public void setPublisherTransport(String publisherTransport) {
        this.publisherTransport = publisherTransport;
    }

    public String getSubscriberQosPolicy() {
        return subscriberQosPolicy;
    }

    public void setSubscriberQosPolicy(String subscriberQosPolicy) {
        this.subscriberQosPolicy = subscriberQosPolicy;
    }

    public String getSubscriberTransport() {
        return subscriberTransport;
    }

    public void setSubscriberTransport(String subscriberTransport) {
        this.subscriberTransport = subscriberTransport;
    }

    public String getTransportType() {
        return transportType;
    }

    public void setTransportType(String transportType) {
        this.transportType = transportType;
    }

    public PrintWriter getLogWriter() {
        return null; // logging disabled
    }

    public void setLogWriter(PrintWriter log) {}

    public Object createConnectionFactory() {
        return createConnectionFactory(null);
    }

    public Object createConnectionFactory(ConnectionManager cxManager) {
        TransportConfigurationFactory tcf =
            new TransportConfigurationFactory(transportType);

        ConnectionRequestInfo cxRequestInfo =
            new ConnectionRequestInfoImpl(domainId,
                new ParticipantQosPolicy(participantQosPolicy),
                new PublisherQosPolicy(publisherQosPolicy),
                tcf.createConfiguration(publisherTransport),
                new SubscriberQosPolicy(subscriberQosPolicy),
                tcf.createConfiguration(subscriberTransport));

        return new ConnectionFactoryImpl(this, cxManager, cxRequestInfo);
    }

    public ManagedConnection createManagedConnection(Subject subject,
                                                     ConnectionRequestInfo cxRequestInfo) throws ResourceException {

        if (!(cxRequestInfo instanceof ConnectionRequestInfoImpl)) {
            throw new IllegalArgumentException();
        }
        return new ManagedConnectionImpl(subject, (ConnectionRequestInfoImpl) cxRequestInfo);
    }

    public ManagedConnection matchManagedConnections(Set connectionSet,
                                                     Subject subject,
                                                     ConnectionRequestInfo cxRequestInfo) throws ResourceException {
        return null;
    }

    @Override
    public int hashCode() {
        return Objects.hashCode(
            domainId,
            participantQosPolicy,
            publisherQosPolicy,
            publisherTransport,
            subscriberQosPolicy,
            subscriberTransport,
            transportType);
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }

        if (!(o instanceof ManagedConnectionFactoryImpl)) {
            return false;
        }

        ManagedConnectionFactoryImpl mcf = (ManagedConnectionFactoryImpl) o;
        return Objects.equals(domainId, mcf.domainId)
            && Objects.equals(participantQosPolicy, mcf.participantQosPolicy)
            && Objects.equals(publisherQosPolicy, mcf.publisherQosPolicy)
            && Objects.equals(publisherTransport, mcf.publisherTransport)
            && Objects.equals(subscriberQosPolicy, mcf.subscriberQosPolicy)
            && Objects.equals(subscriberTransport, mcf.subscriberTransport)
            && Objects.equals(transportType, mcf.transportType);
    }
}
