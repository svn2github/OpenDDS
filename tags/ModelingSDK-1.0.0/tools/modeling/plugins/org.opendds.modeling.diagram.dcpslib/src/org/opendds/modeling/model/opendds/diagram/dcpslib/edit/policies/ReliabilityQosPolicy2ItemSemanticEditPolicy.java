/*
 * (c) Copyright Object Computing, Incorporated. 2005,2010. All rights reserved.
 */
package org.opendds.modeling.model.opendds.diagram.dcpslib.edit.policies;

import java.util.Iterator;

import org.eclipse.emf.ecore.EAnnotation;
import org.eclipse.gef.commands.Command;
import org.eclipse.gmf.runtime.common.core.command.ICompositeCommand;
import org.eclipse.gmf.runtime.diagram.core.commands.DeleteCommand;
import org.eclipse.gmf.runtime.emf.commands.core.command.CompositeTransactionalCommand;
import org.eclipse.gmf.runtime.emf.type.core.commands.DestroyElementCommand;
import org.eclipse.gmf.runtime.emf.type.core.requests.DestroyElementRequest;
import org.eclipse.gmf.runtime.notation.Node;
import org.eclipse.gmf.runtime.notation.View;
import org.opendds.modeling.model.opendds.diagram.dcpslib.edit.parts.Period6EditPart;
import org.opendds.modeling.model.opendds.diagram.dcpslib.edit.parts.ReliabilityQosPolicyMax_blocking_time2EditPart;
import org.opendds.modeling.model.opendds.diagram.dcpslib.part.OpenDDSDcpsLibVisualIDRegistry;
import org.opendds.modeling.model.opendds.diagram.dcpslib.providers.OpenDDSDcpsLibElementTypes;

/**
 * @generated
 */
public class ReliabilityQosPolicy2ItemSemanticEditPolicy extends
		OpenDDSDcpsLibBaseItemSemanticEditPolicy {

	/**
	 * @generated
	 */
	public ReliabilityQosPolicy2ItemSemanticEditPolicy() {
		super(OpenDDSDcpsLibElementTypes.ReliabilityQosPolicy_3046);
	}

	/**
	 * Do not really destroy the element since the compartment holds non-containment
	 * references while GMF expects the compartment to hold contained references.
	 * Therefore a DestroyReferenceCommand is returned instead of a DestroyElementCommand.
	 * @generated NOT
	 */
	protected Command getDestroyElementCommand(DestroyElementRequest req) {
		CompositeTransactionalCommand cmd = new CompositeTransactionalCommand(
				getEditingDomain(), null);
		cmd.setTransactionNestingEnabled(false);
		cmd.add(com.ociweb.gmf.edit.commands.RequestToCommandConverter
				.destroyElementRequestToDestroyReferenceCommand(req, getHost(),
						getEditingDomain()));
		return getGEFWrapper(cmd);
	}

	/**
	 * @generated
	 */
	private void addDestroyChildNodesCommand(ICompositeCommand cmd) {
		View view = (View) getHost().getModel();
		for (Iterator nit = view.getChildren().iterator(); nit.hasNext();) {
			Node node = (Node) nit.next();
			switch (OpenDDSDcpsLibVisualIDRegistry.getVisualID(node)) {
			case ReliabilityQosPolicyMax_blocking_time2EditPart.VISUAL_ID:
				for (Iterator cit = node.getChildren().iterator(); cit
						.hasNext();) {
					Node cnode = (Node) cit.next();
					switch (OpenDDSDcpsLibVisualIDRegistry.getVisualID(cnode)) {
					case Period6EditPart.VISUAL_ID:
						cmd.add(new DestroyElementCommand(
								new DestroyElementRequest(getEditingDomain(),
										cnode.getElement(), false))); // directlyOwned: true
						// don't need explicit deletion of cnode as parent's view deletion would clean child views as well 
						// cmd.add(new org.eclipse.gmf.runtime.diagram.core.commands.DeleteCommand(getEditingDomain(), cnode));
						break;
					}
				}
				break;
			}
		}
	}

}
