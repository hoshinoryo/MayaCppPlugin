#include "CreateArmJointCommand.h"
#include "FuncUtils.h"
#include "PreBuildBoneChain.h"
#include "StatusUtils.h"

#include <maya/MFnIkJoint.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MVector.h>
#include <maya/MString.h>
#include <maya/MQuaternion.h>
#include <maya/MMatrix.h>


namespace
{
    MQuaternion matrixToQuaternion(const MMatrix& matrix)
    {
        MTransformationMatrix transformMatrix(matrix);

        return transformMatrix.rotation();
    }
}


void* CreateArmJoint::creator()
{
    return new CreateArmJoint;
}

MStatus CreateArmJoint::doIt(const MArgList& args)
{
    MStatus status;
    
    if (FuncUtils::objectExists("shoulder_jnt") ||
        FuncUtils::objectExists("elbow_jnt") ||
        FuncUtils::objectExists("hand_jnt")
        )
    {
        MGlobal::displayError("Arm joint chain already exists.");
        return MS::kFailure;
    }

    // Read position from locator guide
    MVector shoulderPosition;
    MVector elbowPosition;
    MVector handPosition;

    status = FuncUtils::getTransformWorldPosition("shoulder_guide", shoulderPosition);
    RETURN_IF_MAYA_FAILED(status, "Cannot read shoulder guide");
    status = FuncUtils::getTransformWorldPosition("elbow_guide", elbowPosition);
    RETURN_IF_MAYA_FAILED(status, "Cannot read elbow guide");
    status = FuncUtils::getTransformWorldPosition("hand_guide", handPosition);
    RETURN_IF_MAYA_FAILED(status, "Cannot read hand guide");

    MMatrix shoulderWorldOrientation;
    MMatrix elbowWorldOrientation;

    status = FuncUtils::buildAimOrientationMatrix(shoulderPosition, elbowPosition, shoulderWorldOrientation);
    RETURN_IF_MAYA_FAILED(status, "Cannot calculate shoulder orientation");
    status = FuncUtils::buildAimOrientationMatrix(elbowPosition, handPosition, elbowWorldOrientation);
    RETURN_IF_MAYA_FAILED(status, "Cannot calculate elbow orientation");

    // childWorld = childLocal * parentWorld
    // -> childLocal = childWorld * inverse(parentWorld)
    const MMatrix shoulderLocalOrientation = shoulderWorldOrientation;
    const MMatrix elbowLocalOrientation = elbowWorldOrientation * shoulderWorldOrientation.inverse();

    const MMatrix handLocalOrientation = elbowWorldOrientation * elbowWorldOrientation.inverse();

    const MQuaternion shoulderJointOrient = matrixToQuaternion(shoulderLocalOrientation);
    const MQuaternion elbowJointOrient = matrixToQuaternion(elbowLocalOrientation);
    const MQuaternion handJointOrient = matrixToQuaternion(handLocalOrientation);

    // Local translation
    const MVector elbowLocalTranslation = (elbowPosition - shoulderPosition) * shoulderWorldOrientation.inverse();
    const MVector handLocalTranslation = (handPosition - elbowPosition) * elbowWorldOrientation.inverse();

    // Create joints
    MFnIkJoint shoulderFn;
    MFnIkJoint elbowFn;
    MFnIkJoint handFn;

    // Create shoulder joint
    MObject shoulderObj = shoulderFn.create(MObject::kNullObj, &status);
    shoulderFn.setName("shoulder_jnt", false);

    // Create elbow joint
    MObject elbowObj = elbowFn.create(shoulderObj, &status);
    elbowFn.setName("elbow_jnt", false);

    // Create hand joint
    MObject handObj = handFn.create(elbowObj, &status);
    handFn.setName("hand_jnt", false, &status);

    // Set position
    status = shoulderFn.setTranslation(shoulderPosition, MSpace::kTransform);
    RETURN_IF_MAYA_FAILED(status, "Failed to position shoulder joint");
    status = elbowFn.setTranslation(elbowLocalTranslation, MSpace::kTransform);
    RETURN_IF_MAYA_FAILED(status, "Failed to position elbow joint");
    status = handFn.setTranslation(handLocalTranslation, MSpace::kTransform);
    RETURN_IF_MAYA_FAILED(status, "Failed to position hand joint");

    // Set joint orientation
    status = shoulderFn.setOrientation(shoulderJointOrient);
    RETURN_IF_MAYA_FAILED(status, "Failed to orient shoulder joint");
    status = elbowFn.setOrientation(elbowJointOrient);
    RETURN_IF_MAYA_FAILED(status, "Failed to orient elbow joint");
    status = handFn.setOrientation(handJointOrient);
    RETURN_IF_MAYA_FAILED(status, "Failed to orient hand joint");

    MGlobal::displayInfo("Arm joint chain created successfully");

    return MS::kSuccess;
}
