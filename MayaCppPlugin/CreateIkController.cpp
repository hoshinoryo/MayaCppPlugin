#include "CreateIkController.h"
#include "ChainCommandUtils.h"
#include "FuncUtils.h"
#include "StatusUtils.h"
#include "ControllerShapeUtils.h"

#include <maya/MDagPath.h>


namespace
{
    constexpr double MIN_VECTOR_LENGTH = 0.000001;
    constexpr double POLE_VECTOR_DISTANCE = 5.0;

    MString ikControllerName(const BoneChainDefinition& chain)
    {
        return chain.prefix() + "_ik_ctrl";
    }

    /*
    MString ikControllerGroupName(const BoneChainDefinition& chain)
    {
        return chain.prefix() + "_ik_ctrl_grp";
    }
    */

    MString poleVectorControllerName(const BoneChainDefinition& chain)
    {
        return chain.prefix() + "_ik_pole_ctrl";
    }

    /*
    MString poleVectorControllerGroupName(const BoneChainDefinition& chain)
    {
        return chain.prefix() + "_ik_pole_ctrl_grp";
    }
    */

    MString ikHandleName(const BoneChainDefinition chain)
    {
        return chain.prefix() + "_ikHandle";
    }
    
    /*
    MString ikHandleGroupName(const BoneChainDefinition chain)
    {
        return chain.prefix() + "_ikHandle_grp";
    }

    
    MStatus createIkController(
        const MString& controllerName,
        const MString& groupName,
        double radius,
        short colorIndex
    )
    {
        MStatus status;
        MString radiusString;
        radiusString.set(radius);

        MString command;
        command.format("circle -name \"^1s\" -normal 1 0 0 -radius ^2s -constructionHistory false",
            controllerName, radiusString);
        status = FuncUtils::executeMayaCommand(command, "Cannot create IK controller circle");

        // Create empty group
        command.format("group -name \"^1s\" \"^2s\"", groupName, controllerName);
        status = FuncUtils::executeMayaCommand(command, "Cannot create curve group");

        MDagPath shapePath;
        status = FuncUtils::getShapeFromTransform(controllerName, shapePath);

        return FuncUtils::setDisplayColor(shapePath.node(), colorIndex);
    }
    */

    MStatus calculatePoleVectorPosition(
        const MString& shoulderJnt,
        const MString& elbowJnt,
        const MString& wristJnt,
        MVector& poleVectorPosition)
    {
        MStatus status;

        MVector shoulderPosition, elbowPosition, wristPosition;

        FuncUtils::getWorldPosition(shoulderJnt, shoulderPosition);
        FuncUtils::getWorldPosition(elbowJnt, elbowPosition);
        FuncUtils::getWorldPosition(wristJnt, wristPosition);

        const MVector shoulderToWrist = wristPosition - shoulderPosition;
        const double chainLengthSquared = shoulderToWrist * shoulderToWrist;

        if (chainLengthSquared < MIN_VECTOR_LENGTH)
        {
            MGlobal::displayError("Shoulder to wrist positions are too close");
            return MS::kFailure;
        }

        // Position of elbow projected on chain between shoulder and wrist
        // Vector projection scale: (a * b) / |b| * |b|
        const MVector shoulderToElbow = elbowPosition - shoulderPosition;
        const double projectionScale = (shoulderToElbow * shoulderToWrist) / chainLengthSquared;
        const MVector projectedPosition = shoulderPosition + shoulderToWrist * projectionScale;

        MVector poleDirection = elbowPosition - projectedPosition;

        // When arm is straight
        if (poleDirection.length() < MIN_VECTOR_LENGTH)
        {
            poleDirection = MVector(0.0, 0.0, -1.0);
        }

        poleDirection.normalize();
        poleVectorPosition = elbowPosition + poleDirection * POLE_VECTOR_DISTANCE;

        return MS::kSuccess;
    }

    MStatus createIkHandle(
        const MString& handleName,
        const MString& handleGroupName,
        const MString& startJoint,
        const MString& endJoint
    )
    {
        MString command;
        command.format("ikHandle -name \"^1s\" -startJoint \"^2s\" -endEffector \"^3s\" -solver \"ikRPsolver\"",
            handleName, startJoint, endJoint);
        FuncUtils::executeMayaCommand(command, "Cannot create arm IK handle");

        // Create empty group
        command.format("group -name \"^1s\" \"^2s\"", handleGroupName, handleName);
        return FuncUtils::executeMayaCommand(command, "Cannot create IK handle group");
    }
}

void* CreateIkController::creator()
{
    return new CreateIkController();
}

MSyntax CreateIkController::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}

MStatus CreateIkController::doIt(const MArgList& args)
{
    MStatus status;

    status = ChainCommandUtils::parseDefinition(syntax(), args, m_Chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");

    if (m_Chain.module != "arm")
    {
        MGlobal::displayError("CreateIkController currently supports only the arm module");
        return MS::kInvalidParameter;
    }

    if (m_Chain.bones.size() != 3)
    {
        MGlobal::displayError("Arm IK requires shoulder, elbow and wrist bones");
        return MS::kFailure;
    }

    // temp
    const MString shoulderJoint = m_Chain.jointName(m_Chain.bones[0], "ik");
    const MString elbowJoint = m_Chain.jointName(m_Chain.bones[1], "ik");
    const MString wristJoint = m_Chain.jointName(m_Chain.bones[2], "ik");

    const MString handleName = ikHandleName(m_Chain);
    const MString controllerName = ikControllerName(m_Chain);
    const MString controllerGroupName = ControllerShapeUtils::controllerGroupName(controllerName);
    const MString poleControllerName = poleVectorControllerName(m_Chain);
    const MString poleGroupName = ControllerShapeUtils::controllerGroupName(poleControllerName);

    if (!FuncUtils::objectExists(shoulderJoint))
    {
        MGlobal::displayError("Missing IK joint: " + shoulderJoint);
        return MS::kFailure;
    }

    if (!FuncUtils::objectExists(elbowJoint))
    {
        MGlobal::displayError("Missing IK joint: " + elbowJoint);
        return MS::kFailure;
    }

    if (!FuncUtils::objectExists(wristJoint))
    {
        MGlobal::displayError("Missing IK joint: " + wristJoint);
        return MS::kFailure;
    }

    if (FuncUtils::objectExists(handleName))
    {
        MGlobal::displayError("IK handle already exists: " + handleName);
        return MS::kFailure;
    }

    if (FuncUtils::objectExists(controllerName) ||
        FuncUtils::objectExists(controllerGroupName))
    {
        MGlobal::displayError("IK controller already exists: " + controllerName);
        return MS::kFailure;
    }

    if (FuncUtils::objectExists(poleControllerName) ||
        FuncUtils::objectExists(poleGroupName))
    {
        MGlobal::displayError("Pole vector controller already exists: " + poleControllerName);
        return MS::kFailure;
    }

    return redoIt();
}

MStatus CreateIkController::redoIt()
{
    MStatus status;

    const MString shoulderJoint = m_Chain.jointName(m_Chain.bones[0], "ik");
    const MString elbowJoint = m_Chain.jointName(m_Chain.bones[1], "ik");
    const MString wristJoint = m_Chain.jointName(m_Chain.bones[2], "ik");

    const MString handleName = ikHandleName(m_Chain);
    const MString handleGroupName = ControllerShapeUtils::controllerGroupName(handleName);
    const MString controllerName = ikControllerName(m_Chain);
    const MString controllerGroupName = ControllerShapeUtils::controllerGroupName(controllerName);
    const MString poleControllerName = poleVectorControllerName(m_Chain);
    const MString poleGroupName = ControllerShapeUtils::controllerGroupName(poleControllerName);

    // Create ik handle
    status = createIkHandle(handleName, handleGroupName, shoulderJoint, wristJoint);
    RETURN_IF_MAYA_FAILED(status, "Unable to create IK handle");


    // Create ik control
    MObject ikControllerTransform;

    status = ControllerShapeUtils::createController(
        controllerName, ControllerShapeUtils::ShapeType::Box, 1.4, ikControllerTransform);
    RETURN_IF_MAYA_FAILED(status, "Unable to create IK controller");

    FuncUtils::matchWorldPositionAndRotation(controllerGroupName, wristJoint);

    MDagPath shapePath;
    FuncUtils::getShapeFromTransform(controllerName, shapePath);
    FuncUtils::setDisplayColor(shapePath.node(), m_Chain.controllerColor); // set color


    // Create pole vector control
    MObject poleControllerTransform;

    status = ControllerShapeUtils::createController(
        poleControllerName, ControllerShapeUtils::ShapeType::Pyramid, 1.0, poleControllerTransform);
    RETURN_IF_MAYA_FAILED(status, "Unable to create pole vector controller");

    MDagPath poleShapePath;
    FuncUtils::getShapeFromTransform(controllerName, poleShapePath);
    FuncUtils::setDisplayColor(poleShapePath.node(), m_Chain.controllerColor); // set color

    MVector poleVectorPosition;
    status = calculatePoleVectorPosition(shoulderJoint, elbowJoint, wristJoint, poleVectorPosition);
    RETURN_IF_MAYA_FAILED(status, "Unable to calculate pole vector position");

    FuncUtils::setWorldPosition(poleGroupName, poleVectorPosition);

    // Parent ikhandle to ik controller
    MString command;
    command.format("parent -absolute \"^1s\" \"^2s\"", handleGroupName, controllerName);
    FuncUtils::executeMayaCommand(command, "Cannot parent IK handle to controller");

    // Orient constraint (ik controller to wrist)
    command.format("orientConstraint -maintainOffset -name \"^1s\" \"^2s\" \"^3s\"",
        wristJoint + "_ik_orientConstraint",
        controllerName,
        wristJoint);
    FuncUtils::executeMayaCommand(command, "Cannot orient constraint wrist Ik joint");

    // Pole vector constraint
    command.format("poleVectorConstraint -name \"^1s\" \"^2s\" \"^3s\"",
        elbowJoint + "_poleVectorConstraint",
        poleControllerName,
        handleName);
    FuncUtils::executeMayaCommand(command, "Cannot create pole vector constraint");

    MGlobal::displayInfo(m_Chain.prefix() + " IK controlllers created successfully");

    return MS::kSuccess;
}

MStatus CreateIkController::undoIt()
{
    return MStatus();
}

bool CreateIkController::isUndoable() const
{
    return true;
}
