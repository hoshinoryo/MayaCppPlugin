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
#include "PreBuildGuide.h"
#include "StatusUtils.h"
#include "ChainCommandUtils.h"
#include "BoneChainDefinition.h"
#include "RigModuleRegistry.h"

#include <vector>
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
    MString modulePrefix(const MString& module, const MString& side)
    {
        if (side.length() == 0 || side == "M")
        {
            return "M_" + module;
        }
        return side + "_" + module;
    }

    MString registeredJointName(const MString& module, const MString& side, const MString& bone, const MString& chainType)
    {
        return modulePrefix(module, side) + "_" + bone + "_" + chainType + "_jnt";
    }

    MStatus findRegisteredBone(const MString& module, const MString& side, const MString& boneLabel, BoneBase& result)
    {
        if (module == "hand")
        {
            TreeBoneDefinition tree;

            MStatus status = RigModuleRegistry::getTree(module, side, tree);
            RETURN_IF_MAYA_FAILED(status, "Cannot read registered tree");

            if (tree.root.label == boneLabel)
            {
                result = tree.root;
                return MS::kSuccess;
            }

            for (const SingleChainDefinition& child : tree.children)
            {
                for (const BoneBase& bone : child.bones)
                {
                    if (bone.label == boneLabel)
                    {
                        result = bone;
                        return MS::kSuccess;
                    }
                }
            }
        }
        else
        {
            SingleChainDefinition chain;

            MStatus status = RigModuleRegistry::getChain(module, side, chain);
            RETURN_IF_MAYA_FAILED(status, "Cannot read registered chain");

            for (const BoneBase& bone : chain.bones)
            {
                if (bone.label == boneLabel)
                {
                    result = bone;
                    return MS::kSuccess;
                }
            }
        }

        MGlobal::displayError("Registered bone not found: " + module + "." + boneLabel);
        return MS::kFailure;
    }


    MStatus resolveRegisteredParent(
        const BoneStructureBase& owner,
        const BoneBase& childBone,
        const MString& childChainType,
        MObject& parentObject,
        MMatrix& parentWorldMatrix
    )
    {
        parentObject = MObject::kNullObj;
        parentWorldMatrix = MMatrix::identity;

        if (!childBone.parent.isValid()) // if has no parent
        {
            return MS::kSuccess;
        }

        const MString parentSide = childBone.parent.side.length() > 0
            ? childBone.parent.side
            : owner.side;

        BoneBase parentBone;
        findRegisteredBone(childBone.parent.module, parentSide, childBone.parent.label, parentBone);

        MString parentChainType = childChainType;
        if (childChainType == "ik" && !parentBone.buildsJointType("ik"))
        {
            parentChainType = "fk";
        }

        const MString parentJointName = registeredJointName(
            childBone.parent.module,
            parentSide,
            childBone.parent.label,
            parentChainType);

        MDagPath parentPath;
        FuncUtils::getDagPath(parentJointName, parentPath);
        parentObject = parentPath.node();
        parentWorldMatrix = parentPath.inclusiveMatrix();

        return MS::kSuccess;
    }



    MStatus createSingleJointChain(
        const SingleChainDefinition& chain,
        const MString& chainType,
        const MObject& initialParent = MObject::kNullObj,
        const MMatrix& initialParentWorldMatrix = MMatrix::identity
    )
    {
        MStatus status;

        const size_t boneChainSize = chain.bones.size();

        if (boneChainSize < 2)
        {
            MGlobal::displayError("Joint chain requires at least two bone definitions");
            return MS::kFailure;
        }

        std::vector<MVector> definitionPositions(boneChainSize);    // world positions
        std::vector<MMatrix> definitionOrientations(boneChainSize); // world orientations
        std::vector<size_t>  buildIndices;


        // Validate and read guide position
        for (size_t i = 0; i < boneChainSize; i++)
        {
            const BoneBase& bone = chain.bones[i];

            const MString guideName = chain.guideName(bone);

            // Read position from locator guide
            status = FuncUtils::getWorldPosition(guideName, definitionPositions[i]);
            RETURN_IF_MAYA_FAILED(status, "Missing or invalid guide: " + guideName);

            if (!bone.buildsJointType(chainType)) continue; // skip bones that do not support the requested joint type

            const MString jointName = chain.jointName(bone, chainType);
            if (FuncUtils::objectExists(jointName))
            {
                MGlobal::displayError("Joint already exists: " + jointName);
                return MS::kFailure;
            }

            buildIndices.push_back(i);
        }

        if (buildIndices.empty())
        {
            MGlobal::displayError(chain.prefix() + " does not define " + chainType + " joints");
            return MS::kInvalidParameter;
        }

        // Calculate orientations
        for (size_t i = 0; i < boneChainSize - 1; i++)
        {
            status = FuncUtils::buildAimOrientationMatrix(
                definitionPositions[i],
                definitionPositions[i + 1],
                definitionOrientations[i]
            );
            RETURN_IF_MAYA_FAILED(status, "Cannot calculate world orientation");
        }

        // The end joint inherits its parent's world orientation
        definitionOrientations[boneChainSize - 1] = definitionOrientations[boneChainSize - 2];

        const size_t buildCount = buildIndices.size();

        std::vector<MVector>     localPositions(buildCount);    // local position
        std::vector<MQuaternion> localOrientations(buildCount); // local orientation matrix

        MObject rootObject      = initialParent;
        MMatrix rootWorldMatrix = initialParentWorldMatrix;

        const size_t    firstIndex     = buildIndices.front();
        const BoneBase& firstBuiltBone = chain.bones[firstIndex];

        if (rootObject.isNull())
        {
            status = resolveRegisteredParent(chain, firstBuiltBone, chainType, rootObject, rootWorldMatrix);
            RETURN_IF_MAYA_FAILED(status, "Cannot resolve chain root parent");
        }

        if (rootObject.isNull())
        {
            localPositions[0]    = definitionPositions[firstIndex];
            localOrientations[0] = FuncUtils::matrixToQuaternion(definitionOrientations[firstIndex]);
        }
        else
        {
            // Position
            // world position -> world point -> local point -> local position
            const MVector& worldPosition = definitionPositions[firstIndex];
            const MPoint worldPoint(worldPosition.x, worldPosition.y, worldPosition.z);
            const MPoint localPoint = worldPoint * rootWorldMatrix.inverse();
            localPositions[0] = MVector(localPoint.x, localPoint.y, localPoint.z);

            // Orientation
            double x, y, z, w;
            MTransformationMatrix parentTransform(rootWorldMatrix);
            parentTransform.getRotationQuaternion(x, y, z, w);
            
            const MMatrix parentOrientation = MQuaternion(x, y, z, w).asMatrix();
            localOrientations[0] = FuncUtils::matrixToQuaternion(
                definitionOrientations[firstIndex]
                * parentOrientation.inverse());
        }

        // childWorld = childLocal * parentWorld
        // -> childLocal = childWorld * inverse(parentWorld)
        for (size_t i = 1; i < buildCount; i++)
        {
            const size_t curIndex  = buildIndices[i];
            const size_t prevIndex = buildIndices[i - 1];

            localPositions[i]    = (definitionPositions[curIndex] - definitionPositions[prevIndex]) * definitionOrientations[prevIndex].inverse();
            localOrientations[i] = FuncUtils::matrixToQuaternion(definitionOrientations[curIndex] * definitionOrientations[prevIndex].inverse());
        }

        MObject parentObject = rootObject;

        for (size_t i = 0; i < buildCount; i++)
        {
            const BoneBase& bone = chain.bones[buildIndices[i]];

            MFnIkJoint jointFn; // Create joints

            MObject jointObject = jointFn.create(parentObject, &status);
            RETURN_IF_MAYA_FAILED(status, "Cannot create joint");

            jointFn.setName(chain.jointName(bone, chainType), false);

            // Set position
            status = jointFn.setTranslation(localPositions[i], MSpace::kTransform);
            RETURN_IF_MAYA_FAILED(status, "Failed to position joint");

            // Set joint orientation
            status = jointFn.setOrientation(localOrientations[i]);
            RETURN_IF_MAYA_FAILED(status, "Failed to orient joint");

            parentObject = jointObject;
        }

        return MS::kSuccess;
    }

    MStatus createTreeJointChain(const TreeBoneDefinition& tree, const MString& chainType)
    {
        MStatus status;

        if (!tree.root.buildsJointType(chainType))
        {
            MGlobal::displayError(tree.prefix() + " does not define a " + chainType + " tree");
            return MS::kInvalidParameter;
        }

        const MString rootGuideName = tree.rootGuideName();
        const MString rootJointName = tree.rootJointName(chainType);

        MVector rootWorldPositon;
        FuncUtils::getWorldPosition(rootGuideName, rootWorldPositon);

        if (FuncUtils::objectExists(rootJointName))
        {
            MGlobal::displayError("Root joint already exists: " + rootJointName);
            return MS::kFailure;
        }

        MObject parentObject;
        MMatrix parentWorldMatrix;
        resolveRegisteredParent(tree, tree.root, chainType, parentObject, parentWorldMatrix);
        
        // world point -> local point
        const MPoint rootWorldPoint(
            rootWorldPositon.x,
            rootWorldPositon.y,
            rootWorldPositon.z
        );
        const MPoint rootLocalPoint = rootWorldPoint * parentWorldMatrix.inverse();

        // Palm joint
        MFnIkJoint rootJointFn; // create root joint
        MObject rootJoint = rootJointFn.create(parentObject, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create root joint");

        rootJointFn.setName(rootJointName, false);
        rootJointFn.setTranslation(MVector(rootLocalPoint.x, rootLocalPoint.y, rootLocalPoint.z), MSpace::kTransform);
        rootJointFn.setOrientation(MQuaternion());

        MDagPath rootJointPath;
        MDagPath::getAPathTo(rootJoint, rootJointPath);
        const MMatrix rootWorldMatrix = rootJointPath.inclusiveMatrix();

        for (const SingleChainDefinition& child : tree.children)
        {
            createSingleJointChain(child, chainType, rootJoint, rootWorldMatrix);
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
