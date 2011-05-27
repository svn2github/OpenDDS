package org.opendds.jms;

import java.util.EventObject;

import javax.jms.Connection;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class ConnectionStateEvent extends EventObject {
    public static final int CONNECTION_STARTED = 1;
    public static final int CONNECTION_STOPPED = 2;

    protected int id;

    public ConnectionStateEvent(Connection source, int id) {
        super(source);

        this.id = id;
    }

    @Override
    public Connection getSource() {
        return (Connection) source;
    }

    public int getId() {
        return id;
    }
}
