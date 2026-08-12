#include "CreateFkController.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "ChainCommandUtils.h"


#include <maya/MGlobal.h>
#include <maya/MDagPath.h>

namespace
{
    MStatus createController(
        const MString& controllerName,
        const MString& groupName,
        const MString& jointName,
        double radius,
        short colorIndex
    )
    {
        MStatus status;

        MString radiusString;
        radiusString.set(radius);

        MString command;

        // Create circle controller
        command.format("circle -name \"^1s\" -normal 1 0 0 -radius ^2s -constructionHistory false",
            controllerName, radiusString);
        status = FuncUtils::executeMayaCommand(command, "Cannot create controller circle");

        // Create empty group
        command.format("group -name \"^1s\" \"^2s\"", groupName, controllerName);
        status = FuncUtils::executeMayaCommand(command, "Cannot create curve group");

        // Match group to joints
        status = FuncUtils::matchWorldPositionAndRotation(groupName, jointName);
        RETURN_IF_MAYA_FAILED(status, "Cannot align controller group");

        MDagPath shapePath;
        FuncUtils::getShapeFromTransform(controllerName, shapePath);

        return FuncUtils::setDisplayColor(shapePath.node(), colorIndex);
    }
}

void* CreateFkController::creator()
{
    return new CreateFkController();
}

MSyntax CreateFkController::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}

MStatus CreateFkController::doIt(const MArgList& args)
{
    MStatus status;
    
    status = ChainCommandUtils::parseDefinition(syntax(), args, m_Chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");

    for (const BoneDefinition& bone : m_Chain.bones)
    {
        const MString jointName = m_Chain.jointName(bone);
        const MString controllerName = m_Chain.controllerName(bone);
        const MString groupName = m_Chain.controllerGroupName(bone);

        if (!FuncUtils::objectExists(jointName))
        {
            MGlobal::displayError("Missing joint: " + jointName);
            return MS::kFailure;
        }

        if (FuncUtils::objectExists(controllerName) ||
            FuncUtils::objectExists(groupName))
        {
            MGlobal::displayError("Controller already exists: " + controllerName);
            return MS::kFailure;
        }
    }

    return redoIt();
}

MStatus CreateFkController::redoIt()
{
    MStatus status;

    for (const BoneDefinition& bone : m_Chain.bones)
    {
        status = createController(
            m_Chain.controllerName(bone),
            m_Chain.controllerGroupName(bone),
            m_Chain.jointName(bone),
            bone.constrollerRadius,
            m_Chain.controllerColor
            );
        RETURN_IF_MAYA_FAILED(status, "Unable to create FK controller");
    }

    // Create FK hierachy
    for (size_t i = 1; i < m_Chain.bones.size(); i++)
    {
        const MString parentController = m_Chain.controllerName(m_Chain.bones[i - 1]);
        const MString childGroup = m_Chain.controllerGroupName(m_Chain.bones[i]);

        MString command;
        command.format("parent -absolute \"^1s\" \"^2s\"", childGroup, parentController);
        FuncUtils::executeMayaCommand(command, "Cannot parent elbow controller");
    }

    // Controller constraint joints
    for (const BoneDefinition& bone : m_Chain.bones)
    {
        const MString controllerName = m_Chain.controllerName(bone);
        const MString jointName = m_Chain.jointName(bone);

        MString command;
        command.format("parentConstraint -maintainOffset -name \"^1s\" \"^2s\" \"^3s\"",
            jointName + "_parentConstriant", controllerName, jointName);
        status = FuncUtils::executeMayaCommand(command,"Cannot create FK parent constraint");
    }

    MGlobal::displayInfo(m_Chain.prefix() + " FK Controllers created successfully");

    return MS::kSuccess;
}

MStatus CreateFkController::undoIt()
{
    return MStatus();
}

bool CreateFkController::isUndoable() const
{
    return true;
}
