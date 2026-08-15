#include "CreateFkController.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "ChainCommandUtils.h"
#include "ControllerShapeUtils.h"


#include <maya/MGlobal.h>
#include <maya/MDagPath.h>

namespace
{
    MString fkControllerName(const BoneChainDefinition& chain, const BoneDefinition& bone)
    {
        return chain.prefix() + "_" + bone.label + "_fk_ctrl";
    }

    /*
    MString fkControllerGroupName(const BoneChainDefinition& chain, const BoneDefinition& bone)
    {
        return ControllerShapeUtils::controllerGroupName(fkControllerName(chain, bone));
    }

    MStatus createFkController(
        const MString& controllerName,
        const MString& jointName,
        double radius,
        short colorIndex
    )
    {
        MStatus status;
        MObject controllerTransform;
        const MString controllerGroupName = ControllerShapeUtils::controllerGroupName(controllerName);

        status = ControllerShapeUtils::createController(
            controllerName,
            ControllerShapeUtils::ShapeType::Circle,
            radius,
            controllerTransform);
        RETURN_IF_MAYA_FAILED(status, "Unable to create FK controller");

        // Match group to joints
        FuncUtils::matchWorldPositionAndRotation(controllerGroupName, jointName);

        MDagPath shapePath;
        FuncUtils::getShapeFromTransform(controllerName, shapePath);

        return FuncUtils::setDisplayColor(shapePath.node(), colorIndex);
    }
    */
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
        const MString jointName = m_Chain.jointName(bone, "fk");
        const MString controllerName = fkControllerName(m_Chain, bone);
        const MString groupName = ControllerShapeUtils::controllerGroupName(controllerName);

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
        const MString jointName = m_Chain.jointName(bone, "fk");
        const MString controllerName = fkControllerName(m_Chain, bone);
        const MString groupName = ControllerShapeUtils::controllerGroupName(controllerName);

        MObject controllerTransform;

        status = ControllerShapeUtils::createController(
            controllerName,
            ControllerShapeUtils::ShapeType::Circle,
            bone.constrollerRadius,
            controllerTransform
            );
        RETURN_IF_MAYA_FAILED(status, "Unable to create FK controller");

        FuncUtils::matchWorldPositionAndRotation(groupName, jointName); // match position

        MDagPath shapePath;
        FuncUtils::getShapeFromTransform(controllerName, shapePath);
        FuncUtils::setDisplayColor(shapePath.node(), m_Chain.controllerColor); // set color
    }

    // Create FK hierachy
    for (size_t i = 1; i < m_Chain.bones.size(); i++)
    {
        const MString parentController = fkControllerName(m_Chain, m_Chain.bones[i - 1]);
        const MString childController = fkControllerName(m_Chain, m_Chain.bones[i]);
        const MString childGroup = ControllerShapeUtils::controllerGroupName(childController);

        MString command;
        command.format("parent -absolute \"^1s\" \"^2s\"", childGroup, parentController);
        FuncUtils::executeMayaCommand(command, "Cannot parent elbow controller");
    }

    // Controller constraint joints
    for (const BoneDefinition& bone : m_Chain.bones)
    {
        const MString jointName = m_Chain.jointName(bone, "fk");
        const MString controllerName = fkControllerName(m_Chain, bone);

        MString command;
        command.format("parentConstraint -maintainOffset -name \"^1s\" \"^2s\" \"^3s\"",
            jointName + "_parentConstraint",
            controllerName,
            jointName);
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
