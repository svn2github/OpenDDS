/*
 * $Id$
 */

package org.opendds.jms.management.annotation;

import java.lang.annotation.Documented;
import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Indicates an MBean constructor.  This is only required for
 * MBeans utilizing a non-default constructor.
 *
 * @author  Steven Stallion
 * @version $Revision$
 *
 * @see     javax.management.MBeanConstructorInfo
 */
@Documented
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.CONSTRUCTOR)
public @interface Constructor {}
