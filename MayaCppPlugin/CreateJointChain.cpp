//-----------------------------------------------------------------------------------------
// Joint chain building process:
//
// 1. Read the world positions of all guides.
// 2. Calculate the world orientation of each joint toward the next joint.
// 3. Convert world orientations into local joint orientations relative to the parent.
// 4. Create the joints in order and build the parent-child hierarchy.
//-----------------------------------------------------------------------------------------

#include "CreateJointChain.h"
#include "FuncUtils.h"
#include "PreBuildBoneChain.h"
#include "StatusUtils.h"
#include "ChainCommandUtils.h"
#include "BoneChainDefinition.h"

#include <maya/MFnIkJoint.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MVector.h>
#include <maya/MString.h>
#include <maya/MQuaternion.h>
#include <maya/MMatrix.h>


void* CreateJointChain::creator()
{
    return new CreateJointChain;
}

MSyntax CreateJointChain::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}

MStatus CreateJointChain::doIt(const MArgList& args)
{
    MStatus status;
    BoneChainDefinition chain;

    status = ChainCommandUtils::parseDefinition(syntax(), args, chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");

    const size_t boneCount = chain.bones.size();
    if (boneCount < 2)
    {
        MGlobal::displayError("A joint chain requires at least two bones");
        return MS::kFailure;
    }

    std::vector<MVector> worldPositions(boneCount);    // world position
    std::vector<MMatrix> worldOrientations(boneCount); // world orientation matrix
    std::vector<MMatrix> localOrientations(boneCount); // local orientation matrix
    std::vector<MQuaternion> jointOrients(boneCount);  // joint orient
    std::vector<MVector> localTranslations(boneCount); // Local translation

    // Validate and read guide position
    for (size_t i = 0; i < boneCount; i++)
    {
        const MString guideName = chain.guideName(chain.bones[i]);
        const MString jointName = chain.jointName(chain.bones[i]);
        
        if (!FuncUtils::objectExists(guideName))
        {
            MGlobal::displayError("Missing guide: " + guideName);
            return MS::kFailure;
        }

        if (FuncUtils::objectExists(jointName))
        {
            MGlobal::displayError("Joint already exists: " + jointName);
            return MS::kFailure;
        }

        // Read position from locator guide
        status = FuncUtils::getTransformWorldPosition(guideName, worldPositions[i]);
        RETURN_IF_MAYA_FAILED(status, "annot read guide position");
    }

    for (size_t i = 0; i < boneCount - 1; i++)
    {
        status = FuncUtils::buildAimOrientationMatrix(
            worldPositions[i],
            worldPositions[i + 1],
            worldOrientations[i]
        );
        RETURN_IF_MAYA_FAILED(status, "Cannot calculate world orientation");
    }

    // The end joint inherits its parent's world orientation
    worldOrientations[boneCount - 1] = worldOrientations[boneCount - 2];

    // Root translation and orientation
    localTranslations[0] = worldPositions[0];
    localOrientations[0] = worldOrientations[0];

    // childWorld = childLocal * parentWorld
    // -> childLocal = childWorld * inverse(parentWorld)
    for (size_t i = 1; i < boneCount; i++)
    {
        localTranslations[i] = (worldPositions[i] - worldPositions[i - 1]) * worldOrientations[i - 1].inverse();
        localOrientations[i] = worldOrientations[i] * worldOrientations[i - 1].inverse();
    }

    for (size_t i = 0; i < boneCount; i++)
    {
        jointOrients[i] = FuncUtils::matrixToQuaternion(localOrientations[i]);
    }

    MObject parentObject = MObject::kNullObj;

    for (size_t i = 0; i < boneCount; i++)
    {
        MFnIkJoint jointFn; // Create joints

        MObject jointObject = jointFn.create(parentObject, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create joint");

        jointFn.setName(chain.jointName(chain.bones[i]), false);

        // Set position
        status = jointFn.setTranslation(localTranslations[i], MSpace::kTransform);
        RETURN_IF_MAYA_FAILED(status, "Failed to position joint");

        // Set joint orientation
        status = jointFn.setOrientation(jointOrients[i]);
        RETURN_IF_MAYA_FAILED(status, "Failed to orient joint");

        parentObject = jointObject;
    }

    MGlobal::displayInfo(chain.prefix() + " joint chain created successfully");

    return MS::kSuccess;
}
