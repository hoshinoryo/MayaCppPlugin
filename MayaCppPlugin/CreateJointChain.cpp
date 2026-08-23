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
#include <maya/MDagPath.h>


namespace
{
    MStatus createSingleJointChain(
        const SingleChainDefinition& chain,
        const MString& chainType,
        const MObject& initialParent = MObject::kNullObj,
        const MMatrix& initialParentWorldMatrix = MMatrix::identity
    )
    {
        MStatus status;

        const size_t boneCount = chain.bones.size();
        /*
        if (boneCount < 2)
        {
            MGlobal::displayError("A joint chain requires at least two bones");
            return MS::kFailure;
        }
        */

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

        if (initialParent.isNull())
        {
            // Root translation and orientation
            localTranslations[0] = worldPositions[0];
            localOrientations[0] = worldOrientations[0];
        }
        else
        {
            const MPoint worldPoint(
                worldPositions[0].x,
                worldPositions[0].y,
                worldPositions[0].z
                );
            const MPoint localPoint = worldPoint * initialParentWorldMatrix.inverse();

            localTranslations[0] = MVector(localPoint.x, localPoint.y, localPoint.z);

            double x, y, z, w;
            MTransformationMatrix parentTransform(initialParentWorldMatrix);
            parentTransform.getRotationQuaternion(x, y, z, w);
            
            const MMatrix parentWorldOrientation = MQuaternion(x, y, z, w).asMatrix();
            localOrientations[0] = worldOrientations[0] * parentWorldOrientation.inverse();
        }

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

        MObject parentObject = initialParent;

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

        return MS::kSuccess;
    }

    MStatus createTreeJointChain(const TreeBoneDefinition& tree, const MString& chainType)
    {
        MStatus status;

        const MString parentPrefix = tree.side.length() == 0 || tree.side == "M"
            ? "M_" + tree.parentModule
            : tree.side + "_" + tree.parentModule;
        const MString parentJointName = parentPrefix + "_" + tree.parentBone + "_" + chainType + "_jnt";

        if (!FuncUtils::objectExists(parentJointName))
        {
            MGlobal::displayError("Missing parent joint: " + parentJointName);
            return MS::kFailure;
        }

        const MString rootGuideName = tree.rootGuideName();
        const MString rootJointName = tree.rootJointName(chainType);

        if (!FuncUtils::objectExists(rootGuideName))
        {
            MGlobal::displayError("Missing root guide: " + rootGuideName);
            return MS::kFailure;
        }
        if (FuncUtils::objectExists(rootJointName))
        {
            MGlobal::displayError("Root joint already exists: " + rootJointName);
            return MS::kFailure;
        }

        MDagPath parentJointPath; // wrist joint
        FuncUtils::getDagPath(parentJointName, parentJointPath);
        const MMatrix parentWorldMatrix = parentJointPath.inclusiveMatrix(); // local to world

        MVector rootWorldPositon;
        FuncUtils::getWorldPosition(rootGuideName, rootWorldPositon);
        const MPoint rootWorldPoint(
            rootWorldPositon.x,
            rootWorldPositon.y,
            rootWorldPositon.z
        );
        const MPoint rootLocalPoint = rootWorldPoint * parentWorldMatrix.inverse();

        // Palm joint
        MFnIkJoint rootJointFn;
        MObject rootJoint = rootJointFn.create(parentJointPath.node(), &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create root joint");

        rootJointFn.setName(rootJointName, false);
        rootJointFn.setTranslation(MVector(rootLocalPoint.x, rootLocalPoint.y, rootLocalPoint.z), MSpace::kTransform);
        rootJointFn.setOrientation(MQuaternion());

        MDagPath rootJointPath;
        MDagPath::getAPathTo(rootJoint, rootJointPath);
        const MMatrix rootWorldMatrix = rootJointPath.inclusiveMatrix();

        for (const SingleChainDefinition& child : tree.children)
        {
            status = createSingleJointChain(child, chainType, rootJoint, rootWorldMatrix);
        }

        return MS::kSuccess;
    }
}

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
    MString module, side;
    MArgDatabase database(syntax(), args);

    status = ChainCommandUtils::parseModuleAndSide(syntax(), args, module, side);
    RETURN_IF_MAYA_FAILED(status, "Unable to read module and side");

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

    if (module == "hand")
    {
        if (chainType != "fk")
        {
            MGlobal::displayError("Hand currently supports FK joints only");
            return MS::kInvalidParameter;
        }

        TreeBoneDefinition tree;

        status = ChainCommandUtils::parseDefinition(syntax(), args, tree);
        RETURN_IF_MAYA_FAILED(status, "Cannot read tree definition");

        status =  createTreeJointChain(tree, chainType);
        RETURN_IF_MAYA_FAILED(status, "Cannot create tree joint chain");

        MGlobal::displayInfo(tree.prefix() + " Fk joint tree created successfully");

        return MS::kSuccess;
    }

    SingleChainDefinition chain;

    status = ChainCommandUtils::parseDefinition(syntax(), args, chain);
    RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");
    
    status = createSingleJointChain(chain, chainType);
    RETURN_IF_MAYA_FAILED(status, "Cannot create single joint chain");

    MGlobal::displayInfo(chain.prefix() + " " + chainType + " joint chain created successfully");

    return MS::kSuccess;
}
