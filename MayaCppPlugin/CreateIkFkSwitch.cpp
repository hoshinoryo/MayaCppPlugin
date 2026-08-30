#include "CreateIkFkSwitch.h"
#include "ControllerShapeUtils.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "ChainCommandUtils.h"

#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MDGModifier.h>
#include <maya/MDagPath.h>


namespace
{
    constexpr double SWITCH_SIZE = 0.3;
    constexpr double SWITCH_OFFSET = 3.0;

    MString switchControllerName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ifk_switch_ctrl";
    }

    MString switchGroupName(const SingleChainDefinition& chain)
    {
        return ControllerShapeUtils::controllerGroupName(switchControllerName(chain));
    }

    MString reverseNodeName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ifk_reverse";
    }

    MString ikControllerName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ik_ctrl";
    }

    MString poleControllerName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ik_pole_ctrl";
    }

    MString fkControllerName(const SingleChainDefinition& chain, const BoneBase& bone)
    {
        return chain.prefix() + "_" + bone.label + "_fk_ctrl";
    }

    MString bindConstraintName(const SingleChainDefinition& chain, const BoneBase& bone)
    {
        return chain.jointName(bone, "bind") + "_parentConstraint";
    }

    const BoneBase* lastIkFkJoint(const SingleChainDefinition& chain)
    {
        const BoneBase* result = nullptr;

        for (const BoneBase& bone : chain.bones)
        {
            if (bone.buildsJointType("fk") && bone.buildsJointType("ik"))
            {
                result = &bone;
            }
        }

        return result;
    }

    bool sameDependencyNode(const MString& firstName, const MString& secondName)
    {
        MObject firstObject, secondObject;

        if (!FuncUtils::getDependencyNode(firstName, firstObject))
        {
            return false;
        }
        if (!FuncUtils::getDependencyNode(secondName, secondObject))
        {
            return false;
        }

        return firstObject == secondObject;
    }

    MStatus visibilityPlug(const MString& transformName, MPlug& result)
    {
        MStatus status;
        MDagPath shapePath;

        FuncUtils::getShapeFromTransform(transformName, shapePath);
        MFnDependencyNode shapeFn(shapePath.node());

        // MObject -> MFnDependencyNode -> findPlug()
        result = shapeFn.findPlug("visibility", true, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot find visibility: " + transformName);

        return MS::kSuccess;
    }
     
    // From driver joint to constraint weight plug
    MStatus constraintWeightPlug(const MString& constraintName, const MString& driverName, MPlug& result)
    {
        MStatus status;
        MStringArray targetNames;
        MStringArray weightAliases;

        MString command;
        command.format("parentConstraint -query -targetList \"^1s\"",
            constraintName); // target list

        status = MGlobal::executeCommand(command, targetNames, false, false);
        RETURN_IF_MAYA_FAILED(status, "Cannot query parent constraint targets: " + constraintName);

        command.format("parentConstraint -query -weightAliasList \"^1s\"",
            constraintName); // weight alias list

        status = MGlobal::executeCommand(command, weightAliases, false, false);
        RETURN_IF_MAYA_FAILED(status, "Cannot query parent constraint weights: " + constraintName);

        if (targetNames.length() != weightAliases.length())
        {
            MGlobal::displayError("Constraint target and weight counts do not match: " + constraintName);
            return MS::kFailure;
        }

        MObject constraintObject;
        FuncUtils::getDependencyNode(constraintName, constraintObject);
        MFnDependencyNode constraintFn(constraintObject);

        for (unsigned int i = 0; i < targetNames.length(); i++)
        {
            if (!sameDependencyNode(targetNames[i], driverName))
            {
                continue;
            }

            result = constraintFn.findPlug(weightAliases[i], true, &status);
            RETURN_IF_MAYA_FAILED(status, "Cannot find constraint weight: " + weightAliases[i]);

            return MS::kSuccess;
        }

        MGlobal::displayError("Driver " + driverName + " is not a target of " + constraintName);
        return MS::kFailure;
    }

    MStatus validateDestinationPlug(const MPlug& plug, const MString& description)
    {
        if (plug.isDestination())
        {
            MGlobal::displayError(description + " already has an incoming connection");
            return MS::kFailure;
        }

        return MS::kSuccess;
    }

    MStatus validateChain(const SingleChainDefinition& chain)
    {
        //MStatus status;

        if (chain.module != "arm" && chain.module != "leg")
        {
            MGlobal::displayError("CreateIkFkSwitch supports only arm and leg");
            return MS::kInvalidParameter;
        }

        const BoneBase* endBone = lastIkFkJoint(chain);
        if (endBone == nullptr)
        {
            MGlobal::displayError(chain.prefix() + " does not contain an IK/FK bone");
            return MS::kFailure;
        }

        const MString controllerName = switchControllerName(chain);
        const MString groupName = switchGroupName(chain);
        const MString reverseName = reverseNodeName(chain);
        if (FuncUtils::objectExists(controllerName) || FuncUtils::objectExists(groupName))
        {
            MGlobal::displayError("IK/FK switch already exists: " + controllerName);
            return MS::kFailure;
        }
        if (FuncUtils::objectExists(reverseName))
        {
            MGlobal::displayError("IK/FK reverse node already exists: " + reverseName);
            return MS::kFailure;
        }

        const MString switchConstraint = groupName + "_parentConstraint";
        if (FuncUtils::objectExists(switchConstraint))
        {
            MGlobal::displayError("Switch constraint already exists: " + switchConstraint);
            return MS::kFailure;
        }

        const MString ikController = ikControllerName(chain);
        const MString poleController = poleControllerName(chain);
        const MString endBindJoint = chain.jointName(*endBone, "bind");
        if (!FuncUtils::objectExists(ikController))
        {
            MGlobal::displayError("Missing IK controller: " + ikController);
            return MS::kFailure;
        }
        if (!FuncUtils::objectExists(poleController))
        {
            MGlobal::displayError("Missing pole vector controller: " + poleController);
            return MS::kFailure;
        }
        if (!FuncUtils::objectExists(endBindJoint))
        {
            MGlobal::displayError("Missing end bind joint: " + endBindJoint);
            return MS::kFailure;
        }

        MPlug plug;
        visibilityPlug(ikController,plug);
        validateDestinationPlug(plug, ikController + ".visibility");
        visibilityPlug(poleController, plug);
        validateDestinationPlug(plug, poleController + ".visibility");

        for (const BoneBase& bone : chain.bones)
        {
            const MString fkController = fkControllerName(chain, bone);
            if (!FuncUtils::objectExists(fkController))
            {
                MGlobal::displayError("Missing FK controller: " + fkController);
                return MS::kFailure;
            }

            visibilityPlug(fkController, plug);
            validateDestinationPlug(plug, fkController + ".visibility");
        }

        for (const BoneBase& bone : chain.bones)
        {
            if (!bone.buildsJointType("fk") || !bone.buildsJointType("ik")) continue;

            const MString fkJoint = chain.jointName(bone, "fk");
            const MString ikJoint = chain.jointName(bone, "ik");
            const MString constraint = bindConstraintName(chain, bone);

            if (!FuncUtils::objectExists(fkJoint))
            {
                MGlobal::displayError("Missing FK joint: " + fkJoint);
                return MS::kFailure;
            }

            if (!FuncUtils::objectExists(ikJoint))
            {
                MGlobal::displayError("Missing IK joint: " + ikJoint);
                return MS::kFailure;
            }

            if (!FuncUtils::objectExists(constraint))
            {
                MGlobal::displayError("Missing bind parent constraint: " + constraint);
                return MS::kFailure;
            }

            constraintWeightPlug(constraint, fkJoint, plug);
            validateDestinationPlug(plug, constraint + " FK weight");
            constraintWeightPlug(constraint, ikJoint, plug);
            validateDestinationPlug(plug, constraint + " IK weight");
        }

        return MS::kSuccess;
    }

    MStatus setAllCurveColors(const MObject& controllerTransform, short colorIndex)
    {
        MStatus status;
        MFnDagNode controllerFn(controllerTransform);

        for (unsigned int i = 0; i < controllerFn.childCount(); i++)
        {
            MObject child = controllerFn.child(i);

            if (!child.hasFn(MFn::kNurbsCurve)) continue;

            FuncUtils::setDisplayColor(child, colorIndex);
        }

        return MS::kSuccess;
    }

    MStatus lockAndHideTransformChannels(const MObject& controllerTransform)
    {
        MStatus status;
        MFnDependencyNode controllerFn(controllerTransform);

        const char* attributes[] =
        {
            "translateX",
            "translateY",
            "translateZ",
            "rotateX",
            "rotateY",
            "rotateZ",
            "scaleX",
            "scaleY",
            "scaleZ"
        };

        for (const char* attributeName : attributes)
        {
            MPlug plug = controllerFn.findPlug(attributeName, true);
            plug.setKeyable(false);
            plug.setChannelBox(false);
            plug.setLocked(true);
        }

        return MS::kSuccess;
    }

    MStatus createIfkAttribute(const MObject& controllerTransform, MPlug& ifkPlug)
    {
        MStatus status;

        MFnNumericAttribute attributeFn;
        MObject attribute = attributeFn.create(
            "IFK", "ifk",
            MFnNumericData::kBoolean,
            0,
            &status
        );
        RETURN_IF_MAYA_FAILED(status, "Cannot create IFK attribute");

        attributeFn.setKeyable(true);

        MFnDependencyNode controllerFn(controllerTransform);
        status = controllerFn.addAttribute(attribute);
        RETURN_IF_MAYA_FAILED(status, "Cannot add IFK attribute");

        ifkPlug = MPlug(controllerTransform, attribute);

        return MS::kSuccess;
    }

    MStatus connectVisibility(const MPlug& source, const MString& controllerName, MDGModifier& modifier)
    {
        MStatus status;
        MPlug destination;

        visibilityPlug(controllerName, destination);

        status = modifier.connect(source, destination);
        RETURN_IF_MAYA_FAILED(status, "Cannot connect controller visibility");

        return MS::kSuccess;
    }

    MStatus createSwitchConstraint(const SingleChainDefinition& chain, const BoneBase& endBone)
    {
        const MString bindJoint = chain.jointName(endBone, "bind");
        const MString groupName = switchGroupName(chain);
        const MString constraintName = groupName + "_parentConstraint";

        MString command;
        command.format("parentConstraint -maintainOffset -name \"^1s\" \"^2s\" \"^3s\"",
            constraintName,
            bindJoint,
            groupName);

        return FuncUtils::executeMayaCommand(command, "Cannot constrain IK/FK switch group");
    }
}



void* CreateIkFkSwitch::creator()
{
    return new CreateIkFkSwitch();
}

MSyntax CreateIkFkSwitch::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}

MStatus CreateIkFkSwitch::doIt(const MArgList& args)
{
    MStatus status;

    status = ChainCommandUtils::parseDefinition(syntax(), args, m_Chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot read IK/FK chain definition");

    status = validateChain(m_Chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot validate IK/FK switch");

    return redoIt();
}

MStatus CreateIkFkSwitch::redoIt()
{
    MStatus status;
    const BoneBase* endBone = lastIkFkJoint(m_Chain);

    if (endBone == nullptr)
    {
        return MS::kFailure;
    }

    const MString switchName = switchControllerName(m_Chain);
    const MString switchGroup = switchGroupName(m_Chain);
    const MString reverseName = reverseNodeName(m_Chain);
    const MString bindJoint = m_Chain.jointName(*endBone, "bind");

    MObject controllerTransform;

    status = ControllerShapeUtils::createController(
        switchName,
        ControllerShapeUtils::ShapeType::Sphere,
        SWITCH_SIZE,
        controllerTransform
    );
    RETURN_IF_MAYA_FAILED(status, "Cannot create IK/FK switch controller");

    setAllCurveColors(controllerTransform,m_Chain.controllerColor);
    FuncUtils::matchWorldPositionAndRotation(switchGroup, bindJoint);

    // Move away from the skeleton
    MVector switchPosition;
    FuncUtils::getWorldPosition(switchGroup, switchPosition);

    const double sideDirection = m_Chain.side == "R" ? -1.0 : 1.0;
    switchPosition.z += SWITCH_OFFSET * sideDirection;

    FuncUtils::setWorldPosition(switchGroup, switchPosition);

    // Constraint
    status = createSwitchConstraint(m_Chain, *endBone);
    RETURN_IF_MAYA_FAILED(status, "Cannot constrain IK/FK switch");

    lockAndHideTransformChannels(controllerTransform);

    MPlug ifkPlug;
    createIfkAttribute(controllerTransform, ifkPlug);

    // Create reverse node
    MFnDependencyNode reverseFn;
    MObject reverseObject = reverseFn.create("reverse");
    reverseFn.setName(reverseName, false);

    MPlug reverseInput = reverseFn.findPlug("inputX", true, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot find reverse inputX");
    MPlug reverseOutput = reverseFn.findPlug("outputX", true, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot find reverse outputX");

    MDGModifier modifier;

    // IFK -> reverse.inputX
    status = modifier.connect(ifkPlug, reverseInput);
    RETURN_IF_MAYA_FAILED(status, "Cannot connect IFK to reverse node");
    // IFK -> IK controller visibility
    status = connectVisibility(ifkPlug, ikControllerName(m_Chain), modifier);
    RETURN_IF_MAYA_FAILED(status, "Cannot connect IK controller visibility");
    // IFK -> pole vector visibility
    status = connectVisibility(ifkPlug, poleControllerName(m_Chain), modifier);
    RETURN_IF_MAYA_FAILED(status, "Cannot connect pole vector visibility");

    // reverse.outputX -> every FK controller visibility
    for (const BoneBase& bone : m_Chain.bones)
    {
        status = connectVisibility(reverseOutput, fkControllerName(m_Chain, bone), modifier);
        RETURN_IF_MAYA_FAILED(status, "Cannot connect FK controller visibility");
    }

    // Connect constraint weights only for bones that have both FK and IK.
    for (const BoneBase& bone : m_Chain.bones)
    {
        if (!bone.buildsJointType("fk") || !bone.buildsJointType("ik")) continue;

        const MString constraintName = bindConstraintName(m_Chain, bone);

        const MString fkJoint = m_Chain.jointName(bone, "fk");
        const MString ikJoint = m_Chain.jointName(bone, "ik");

        MPlug fkWeight, ikWeight;

        status = constraintWeightPlug(constraintName, fkJoint, fkWeight);
        RETURN_IF_MAYA_FAILED(status, "Cannot get FK constraint weight");

        status = constraintWeightPlug(constraintName, ikJoint, ikWeight);
        RETURN_IF_MAYA_FAILED(status, "Cannot get IK constraint weight");

        // IFK -> IK bind constraint weight
        status = modifier.connect(ifkPlug, ikWeight);
        RETURN_IF_MAYA_FAILED(status, "Cannot connect IK constraint weight");

        // reverse.outputX -> FK bind constraint weight
        status = modifier.connect(reverseOutput, fkWeight);
        RETURN_IF_MAYA_FAILED(status, "Cannot connect FK constraint weight");
    }

    status = modifier.doIt();
    RETURN_IF_MAYA_FAILED(status, "Cannot apply IK/FK connections");

    MGlobal::displayInfo(m_Chain.prefix() + " IK/FK switch created successfully");
    return MS::kSuccess;
}

MStatus CreateIkFkSwitch::undoIt()
{
    return MStatus();
}

bool CreateIkFkSwitch::isUndoable() const
{
    return true;
}
