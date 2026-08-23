//-----------------------------------------------------------------------------------------
// Joint chain building process:
//
// 1. Read the world positions of all guides.
// 2. Calculate the world orientation of each joint toward the next joint.
// 3. Convert world orientations into local joint orientations relative to the parent.
// 4. Create the joints in order and build the parent-child hierarchy.
//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------
// CreateJointChain-specific flag: chainType
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
#include <maya/MArgDatabase.h>


void* CreateJointChain::creator()
{
    return new CreateJointChain;
}

MSyntax CreateJointChain::newSyntax()
{
    MSyntax syntax = ChainCommandUtils::createSyntax();
    syntax.addFlag("-ct", "-chainType", MSyntax::kString);

    return syntax;
}

MStatus CreateJointChain::doIt(const MArgList& args)
{
    MStatus status;
    BoneChainDefinition chain;

    status = ChainCommandUtils::parseDefinition(syntax(), args, chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");

    if (chain.module != "arm" && chain.module != "leg" && chain.module != "spine" && chain.module != "head")
    {
        MGlobal::displayError("Joint chains currently unsupported");
        return MS::kInvalidParameter;
    }

    MArgDatabase database(syntax(), args, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot read command arguments");
    
    // Get chain type
    MString chainType;
    if (!database.isFlagSet("-chainType"))
    {
        MGlobal::displayError("chainType is required. Use fk or ik");
        return MS::kInvalidParameter;
    }
    database.getFlagArgument("-chainType", 0, chainType);
    chainType.toLowerCase();

    if (chainType != "fk" && chainType != "ik")
    {
        MGlobal::displayError("chainType must be fk or ik");
        return MS::kInvalidParameter;
    }

    if ((chain.module == "spine" && chainType == "ik") || (chain.module == "head" && chainType == "ik"))
    {
        MGlobal::displayError("Current module currently supports FK");
        return MS::kInvalidParameter;
    }


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
        const MString jointName = chain.jointName(chain.bones[i], chainType);
        
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
        status = FuncUtils::getWorldPosition(guideName, worldPositions[i]);
        RETURN_IF_MAYA_FAILED(status, "Cannot read guide position");
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

        jointFn.setName(chain.jointName(chain.bones[i], chainType), false);

        // Set position
        status = jointFn.setTranslation(localTranslations[i], MSpace::kTransform);
        RETURN_IF_MAYA_FAILED(status, "Failed to position joint");

        // Set joint orientation
        status = jointFn.setOrientation(jointOrients[i]);
        RETURN_IF_MAYA_FAILED(status, "Failed to orient joint");

        parentObject = jointObject;
    }

    MGlobal::displayInfo(chain.prefix() + " " + chainType + " joint chain created successfully");

    return MS::kSuccess;
}
