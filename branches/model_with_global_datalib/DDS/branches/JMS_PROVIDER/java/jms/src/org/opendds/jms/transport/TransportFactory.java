/*
 * $
 */

package org.opendds.jms.transport;

import java.util.Properties;

import javax.jms.JMSException;

import OpenDDS.DCPS.transport.TheTransportFactory;
import OpenDDS.DCPS.transport.TransportConfiguration;
import OpenDDS.DCPS.transport.TransportImpl;

import org.opendds.jms.common.beans.BeanHelper;
import org.opendds.jms.common.util.Logger;
import org.opendds.jms.common.util.PropertiesHelper;
import org.opendds.jms.common.util.Serial;

/**
 * @author Steven Stallion
 * @version $Revision$
 */
public class TransportFactory {
    private static final Serial serial = new Serial();

    private String type;
    private Properties properties;

    public TransportFactory(String type, String value) {
        this(type, PropertiesHelper.valueOf(value));
    }

    public TransportFactory(String type, Properties properties) {
        assert type != null;
        assert properties != null;
        
        this.type = type;
        this.properties = properties;
    }

    protected TransportConfiguration createConfiguration() throws JMSException {
        TransportConfiguration configuration;

        synchronized (serial) {
            if (serial.overflowed()) {
                throw new JMSException("Insufficient Transport IDs available");
            }
            configuration = TheTransportFactory.get_or_create_configuration(serial.next(), type);
        }

        Logger logger = Transports.getLogger(configuration);
        logger.debug("Configuring %s %s", configuration, properties);

        if (!properties.isEmpty()) {
            BeanHelper helper = new BeanHelper(configuration.getClass());
            helper.setProperties(configuration, properties);
        }

        return configuration;
    }

    public TransportImpl createTransport() throws JMSException {
        TransportConfiguration configuration = createConfiguration();

        TransportImpl transport = TheTransportFactory.create_transport_impl(configuration.getId(), false);
        if (transport == null) {
            throw new JMSException("Unable to create Transport; please check logs");
        }
        transport.configure(configuration);

        Logger logger = Transports.getLogger(configuration);
        logger.debug("Created %s", transport);

        return transport;
    }
}
