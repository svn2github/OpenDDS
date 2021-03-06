/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

package i2jrt;

public abstract class TAOLocalObject extends TAOObject {

  protected TAOLocalObject(long ptr) {
    super(ptr);
  }

  public boolean _non_existent() {
    return false;
  }

}
