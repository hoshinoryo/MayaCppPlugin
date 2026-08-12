#include "CreateFkController.h"
#include "StatusUtils.h"
#include "FuncUtils.h"

#include <maya/MGlobal.h>
#include <maya/MDagPath.h>

namespace
{
    MStatus createController(
        const MString& controllerName,
        const MString& groupName,
        const MString& jointName,
        double radius = 1.0
    )
    {
        MStatus status;

        if (FuncUtils::objectExists(controllerName) || FuncUtils::objectExists(groupName))
        {
            MGlobal::displayError("Controller already exists: " + controllerName);
            return MS::kFailure;
        }

        // Create circle controller
        MString radiusString;
        radiusString.set(radius);
        MString createCircleCommand("circle -name \"^1s\" -normal 1 0 0 -radius ^2s -constructionHistory false");
        MString cmd;
        cmd.format(createCircleCommand, controllerName, radiusString);
        status = FuncUtils::executeMayaCommand(cmd, "Cannot create controller circle");

        // Create empty group
        MString groupCommand("group -name \"^1s\" \"^2s\"");
        cmd.format(groupCommand, groupName, controllerName);
        status = FuncUtils::executeMayaCommand(cmd, "Cannot create curve group");

        // Match group to joints
        status = FuncUtils::matchWorldPositionAndRotation(groupName, jointName);
        RETURN_IF_MAYA_FAILED(status, "Cannot align controller group");

        MDagPath curvePath;
        FuncUtils::getShapeFromTransform(controllerName, curvePath);
        MObject curveObject = curvePath.node();
        FuncUtils::setDisplayColor(curveObject, 4);

        return MS::kSuccess;
    }
}

void* CreateFkController::creator()
{
    return new CreateFkController();
}

MStatus CreateFkController::doIt(const MArgList& args)
{
    MStatus status;

    constexpr unsigned int CONTROLLER_COUNT = 3;

    const MString jointNames[CONTROLLER_COUNT] =
    {
        "shoulder_jnt",
        "elbow_jnt",
        "hand_jnt"
    };

    const MString controllerNames[CONTROLLER_COUNT] =
    {
        "shoulder_ctrl",
        "elbow_ctrl",
        "hand_ctrl"
    };

    const MString groupNames[CONTROLLER_COUNT] =
    {
        "shoulder_ctrl_grp",
        "elbow_ctrl_grp",
        "hand_ctrl_grp"
    };
    
    // Check before creating
    for (unsigned int i = 0; i < CONTROLLER_COUNT; i++)
    {
        if (!FuncUtils::objectExists(jointNames[i]))
        {
            MGlobal::displayError("Missing joint: " + jointNames[i]);
            return MS::kFailure;
        }

        if (FuncUtils::objectExists(controllerNames[i]) ||
            FuncUtils::objectExists(groupNames[i])
            )
        {
            MGlobal::displayError("Controller already exists: " + controllerNames[i]);
            return MS::kFailure;
        }
    }

    for (unsigned int i = 0; i < CONTROLLER_COUNT; i++)
    {
        status = createController(controllerNames[i], groupNames[i], jointNames[i]);
        RETURN_IF_MAYA_FAILED(status, "Unable to create FK controller");
    }

    // Create FK hierachy
    MString command;
    command.format("parent -absolute \"^1s\" \"^2s\"",groupNames[1], controllerNames[0]);
    FuncUtils::executeMayaCommand(command, "Cannot parent elbow controller");

    command.format("parent -absolute \"^1s\" \"^2s\"", groupNames[2], controllerNames[1]);
    FuncUtils::executeMayaCommand(command, "Cannot parent hand controller");

    // Controller constraint joints
    for (unsigned int i = 0; i < CONTROLLER_COUNT; i++)
    {
        const MString constraintName = controllerNames[i] + "_parentConstraint";
        MString constraintCommand("parentConstraint -maintainOffset -name \"^1s\" \"^2s\" \"^3s\"");
        command.format(constraintCommand, constraintName, controllerNames[i], jointNames[i]);
        
        status = FuncUtils::executeMayaCommand(command, "Cannot create parent constraint");
    }

    MGlobal::displayInfo("FK Controllers created successfully");

    return MS::kSuccess;
}
