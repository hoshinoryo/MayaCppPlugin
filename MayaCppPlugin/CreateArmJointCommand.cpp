#include "CreateArmJointCommand.h"

#include <maya/MFnIkJoint.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MVector.h>
#include <maya/MString.h>


MStatus checkStatusAndReturnIfFail(const MStatus& status, const MString& message)
{
    if (!status)
    {
        MGlobal::displayError(message + ": " + status.errorString());
        return status;
    }
    return MS::kSuccess;
}


void* CreateArmJointCommand::creator()
{
    return new CreateArmJointCommand;
}

MStatus CreateArmJointCommand::doIt(const MArgList& args)
{
    MStatus status;

    MFnIkJoint shoulderFn;
    MFnIkJoint elbowFn;
    MFnIkJoint handFn;

    // Create shoulder joint
    MObject shoulderObj = shoulderFn.create(MObject::kNullObj, &status);
    if (!checkStatusAndReturnIfFail(status, "Failed to create shoulder joint")) return status;
    MGlobal::displayInfo("Shoulder created");

    shoulderFn.setName("shoulder_jnt", false, &status);
    if (!checkStatusAndReturnIfFail(status, "Failed to name shoulder joint")) return status;
    
    status = shoulderFn.setTranslation(MVector(0.0, 10.0, 0.0), MSpace::kTransform);
    if (!checkStatusAndReturnIfFail(status, "setTranslation failed: ")) return status;

    // Create elbow joint
    MObject elbowObj = elbowFn.create(shoulderObj, &status);
    if (!checkStatusAndReturnIfFail(status, "Failed to create elbow joint")) return status;
    MGlobal::displayInfo("Elbow created");

    elbowFn.setName("elbow_jnt", false, &status);
    if (!checkStatusAndReturnIfFail(status, "Failed to name elbow joint")) return status;

    status = elbowFn.setTranslation(MVector(5.0, 0.0, 0.0), MSpace::kTransform);
    if (!checkStatusAndReturnIfFail(status, "Failed to position elbow joint")) return status;

    // Create hand joint
    MObject handObj = handFn.create(elbowObj, &status);
    if (!checkStatusAndReturnIfFail(status, "Failed to create hand joint")) return status;
    MGlobal::displayInfo("Hand created");

    handFn.setName("hand_jnt", false, &status);
    if (!checkStatusAndReturnIfFail(status, "Failed to name hand joint")) return status;

    status = handFn.setTranslation(MVector(4.0, 0.0, 0.0), MSpace::kTransform);
    if (!checkStatusAndReturnIfFail(status, "Failed to position hand joint")) return status;

    MGlobal::displayInfo("Arm joint chain created successfully");

    return MS::kSuccess;
}
