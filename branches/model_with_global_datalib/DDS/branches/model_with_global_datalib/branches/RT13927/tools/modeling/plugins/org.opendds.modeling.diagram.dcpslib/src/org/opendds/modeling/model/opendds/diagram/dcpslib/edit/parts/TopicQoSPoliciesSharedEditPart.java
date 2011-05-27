/*
 * (c) Copyright Object Computing, Incorporated. 2005,2010. All rights reserved.
 */
package org.opendds.modeling.model.opendds.diagram.dcpslib.edit.parts;

import org.eclipse.draw2d.IFigure;
import org.eclipse.emf.common.notify.Notification;
import org.eclipse.emf.ecore.EClass;
import org.eclipse.emf.ecore.EPackage;
import org.eclipse.gef.EditPolicy;
import org.eclipse.gmf.runtime.diagram.ui.editparts.ListCompartmentEditPart;
import org.eclipse.gmf.runtime.diagram.ui.editpolicies.CreationEditPolicy;
import org.eclipse.gmf.runtime.diagram.ui.editpolicies.DragDropEditPolicy;
import org.eclipse.gmf.runtime.diagram.ui.editpolicies.EditPolicyRoles;
import org.eclipse.gmf.runtime.diagram.ui.editpolicies.ResizableCompartmentEditPolicy;
import org.eclipse.gmf.runtime.diagram.ui.figures.ResizableCompartmentFigure;
import org.eclipse.gmf.runtime.notation.View;
import org.opendds.modeling.model.opendds.OpenDDSPackage;
import org.opendds.modeling.model.opendds.diagram.dcpslib.edit.policies.TopicQoSPoliciesSharedCanonicalEditPolicy;
import org.opendds.modeling.model.opendds.diagram.dcpslib.edit.policies.TopicQoSPoliciesSharedItemSemanticEditPolicy;
import org.opendds.modeling.model.opendds.diagram.dcpslib.part.Messages;

/**
 * @generated
 */
public class TopicQoSPoliciesSharedEditPart extends ListCompartmentEditPart {

	/**
	 * @generated
	 */
	public static final int VISUAL_ID = 7047;

	/**
	 * @generated
	 */
	public TopicQoSPoliciesSharedEditPart(View view) {
		super(view);
	}

	/**
	 * @generated
	 */
	protected boolean hasModelChildrenChanged(Notification evt) {
		return false;
	}

	/**
	 * @generated
	 */
	public String getCompartmentName() {
		return Messages.TopicQoSPoliciesSharedEditPart_title;
	}

	/**
	 * @generated
	 */
	public IFigure createFigure() {
		ResizableCompartmentFigure result = (ResizableCompartmentFigure) super
				.createFigure();
		result.setTitleVisibility(false);
		return result;
	}

	/**
	 * @generated
	 */
	protected void createDefaultEditPolicies() {
		super.createDefaultEditPolicies();
		installEditPolicy(EditPolicy.PRIMARY_DRAG_ROLE,
				new ResizableCompartmentEditPolicy());
		installEditPolicy(EditPolicyRoles.SEMANTIC_ROLE,
				new TopicQoSPoliciesSharedItemSemanticEditPolicy());
		installEditPolicy(EditPolicyRoles.CREATION_ROLE,
				new CreationEditPolicy());
		installEditPolicy(EditPolicyRoles.DRAG_DROP_ROLE,
				new DragDropEditPolicy());
		installEditPolicy(EditPolicyRoles.CANONICAL_ROLE,
				new TopicQoSPoliciesSharedCanonicalEditPolicy());
	}

	/**
	 * @generated
	 */
	protected void setRatio(Double ratio) {
		// nothing to do -- parent layout does not accept Double constraints as ratio
		// super.setRatio(ratio); 
	}

	/**
	 * Filter out policies that are owned in the DcpsLib that (ultimately) owns the domain entity.
	 * @generated NOT
	 */
	@Override
	protected java.util.List getModelChildren() {
		return CompartmentChildrenFinder.filterPoliciesOwnedByDcpsLib(super
				.getModelChildren());
	}

}
