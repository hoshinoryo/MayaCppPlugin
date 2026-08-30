#include "MirrorJointChain.h"
#include "ChainCommandUtils.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "RigModuleRegistry.h"

#include <maya/MDagPath.h>
#include <maya/MMatrix.h>


namespace
{
    struct DetachedJoint
    {
        MString joint;
        MString parent;
    };



    MString oppositeSide(const MString& sourceSide)
    {
        if (sourceSide == "L")
        {
            return "R";
        }

        if (sourceSide == "R")
        {
            return "L";
        }

        return "";
    }

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

    MStatus resolveParentJointName(
        const BoneStructureBase& owner,
        const BoneBase& childBone,
        const MString& childChainType,
        MString& result
    )
    {
        result = "";

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

        result = registeredJointName(
            childBone.parent.module,
            parentSide,
            childBone.parent.label,
            parentChainType
        );

        return MS::kSuccess;
    }


    void getDriverBones(
        const SingleChainDefinition& chain,
        const MString& chainType,
        std::vector<const BoneBase*>& result
    )
    {
        result.clear();

        for (const BoneBase& bone : chain.bones)
        {
            if (bone.buildsJointType(chainType))
            {
                result.push_back(&bone);
            }
        }
    }

    MStatus parentToWorld(const MString& joint)
    {
        MString command;
        command.format("parent -absolute -world \"^1s\"", joint);

        return FuncUtils::executeMayaCommand(command, "Cannot unparent joint: " + joint);
    }

    MStatus parentJoint(const MString& child, const MString& parent)
    {
        MString command;
        command.format("parent -absolute \"^1s\" \"^2s\"",
            child,
            parent);

        return FuncUtils::executeMayaCommand(command, "Cannot parent joint: " + child);
    }


    MStatus mirrorJointHierarchy(
        const MString& sourceRootJoint,
        const MString& targetRootJoint,
        const MString& sourcePrefix,
        const MString& targetPrefix
    )
    {
        if (!FuncUtils::objectExists(sourceRootJoint))
        {
            MGlobal::displayError("Source root joint does not exists: " + sourceRootJoint);
            return MS::kFailure;
        }
        if (FuncUtils::objectExists(targetRootJoint))
        {
            MGlobal::displayError("Target root joint already exists: " + targetRootJoint);
            return MS::kFailure;
        }

        // Maya mirror command
        MString command;
        command.format("mirrorJoint -mirrorYZ -mirrorBehavior -searchReplace \"^1s\" \"^2s\" \"^3s\"",
            sourcePrefix,
            targetPrefix,
            sourceRootJoint);

        return FuncUtils::executeMayaCommand(command, "Failed to create mirror joint");
    }

    // Find fk parent of ik bones
    MStatus collectFkAttachedIkRoots(const SingleChainDefinition& chain, std::vector<DetachedJoint>& result)
    {
        result.clear();

        for (const BoneBase& bone : chain.bones)
        {
            if (!bone.buildsJointType("ik") || !bone.parent.isValid()) continue;

            const MString parentSide = bone.parent.side.length() > 0
                ? bone.parent.side
                : chain.side;

            BoneBase parentBone;
            findRegisteredBone(bone.parent.module, parentSide, bone.parent.label, parentBone);
            if (parentBone.buildsJointType("ik")) continue;

            const MString ikRootJoint = chain.jointName(bone, "ik"); // find ik root joint
            if (!FuncUtils::objectExists(ikRootJoint)) continue;

            MString parentJointName;
            resolveParentJointName(chain, bone, "ik", parentJointName);

            result.push_back({ ikRootJoint, parentJointName });
        }
        
        return MS::kSuccess;
    }

    /// <summary>
/// Detach joint hierarchy temporarily.
/// </summary>
    MStatus detachJoints(const std::vector<DetachedJoint>& joints)
    {
        for (const DetachedJoint item : joints)
        {
            MStatus status = parentToWorld(item.joint);
            RETURN_IF_MAYA_FAILED(status, "Cannot detach driver branch");
        }

        return MS::kSuccess;
    }

    /// <summary>
    /// Reattach original joint hierarchy.
    /// </summary>
    MStatus reattachJoints(const std::vector<DetachedJoint>& joints)
    {
        for (const DetachedJoint item : joints)
        {
            MStatus status = parentJoint(item.joint, item.parent);
            RETURN_IF_MAYA_FAILED(status, "Cannot restore driver branch");
        }

        return MS::kSuccess;
    }

    MStatus mirrorSingleChain(const MString& module, const MString& sourceSide, const MString& targetSide, const MString& chainType)
    {
        MStatus status;

        SingleChainDefinition sourceChain, targetChain;

        RigModuleRegistry::getChain(module, sourceSide, sourceChain);
        RigModuleRegistry::getChain(module, targetSide, targetChain);

        std::vector<const BoneBase*> sourceBones;
        std::vector<const BoneBase*> targetBones;
        getDriverBones(sourceChain, chainType, sourceBones);
        getDriverBones(targetChain, chainType, targetBones);

        if (sourceBones.empty() || targetBones.empty())
        {
            MGlobal::displayError(module + " does not define " + chainType + " joints");
            return MS::kInvalidParameter;
        }

        const BoneBase& sourceRootBone = *sourceBones.front();
        const BoneBase& targetRootBone = *targetBones.front();
        const MString sourceRootJoint = sourceChain.jointName(sourceRootBone, chainType);
        const MString targetRootJoint = targetChain.jointName(targetRootBone, chainType);

        MString sourceParentJoint, targetParentJoint;
        resolveParentJointName(sourceChain, sourceRootBone, chainType, sourceParentJoint);
        resolveParentJointName(targetChain, targetRootBone, chainType, targetParentJoint);

        std::vector<DetachedJoint> detachedIkRoots;
        if (chainType == "fk")
        {
            collectFkAttachedIkRoots(sourceChain, detachedIkRoots);
            detachJoints(detachedIkRoots); // detach ik branch
        }

        status = parentToWorld(sourceRootJoint); // detach fk root
        if (!status) // if failed
        {
            reattachJoints(detachedIkRoots);
            return status;
        }

        // Mirror joints
        const MStatus mirrorStatus = mirrorJointHierarchy(
            sourceRootJoint,
            targetRootJoint,
            modulePrefix(module, sourceSide),
            modulePrefix(module, targetSide)
        );

        MStatus restoreSourceStatus = MS::kSuccess;
        if (sourceParentJoint.length() > 0)
        {
            restoreSourceStatus = parentJoint(sourceRootJoint, sourceParentJoint);
        }

        reattachJoints(detachedIkRoots);

        RETURN_IF_MAYA_FAILED(restoreSourceStatus, "Cannot restore source root");
        RETURN_IF_MAYA_FAILED(mirrorStatus, "Unable to mirror driver chain");

        if (targetParentJoint.length() > 0)
        {
            parentJoint(targetRootJoint, targetParentJoint);
        }

        return MS::kSuccess;
    }

    MStatus mirrorTree(const MString& module, const MString& sourceSide, const MString& targetSide, const MString& chainType)
    {
        MStatus status;

        TreeBoneDefinition sourceTree, targetTree;

        RigModuleRegistry::getTree(module, sourceSide, sourceTree);
        RigModuleRegistry::getTree(module, targetSide, targetTree);

        const MString sourceRootJoint = sourceTree.rootJointName(chainType);
        const MString targetRootJoint = targetTree.rootJointName(chainType);

        MString sourceParentJoint, targetParentJoint;
        resolveParentJointName(sourceTree, sourceTree.root, chainType, sourceParentJoint);
        resolveParentJointName(targetTree, targetTree.root, chainType, targetParentJoint);

        status = parentToWorld(sourceRootJoint); // detach fk root

        // Mirror joints
        const MStatus mirrorStatus = mirrorJointHierarchy(
            sourceRootJoint,
            targetRootJoint,
            modulePrefix(module, sourceSide),
            modulePrefix(module, targetSide)
        );

        MStatus restoreSourceStatus = MS::kSuccess;
        if (sourceParentJoint.length() > 0)
        {
            restoreSourceStatus = parentJoint(sourceRootJoint, sourceParentJoint);
        }

        RETURN_IF_MAYA_FAILED(restoreSourceStatus, "Cannot restore source root");
        RETURN_IF_MAYA_FAILED(mirrorStatus, "Unable to mirror driver chain");

        parentJoint(targetRootJoint, targetParentJoint);

        return MS::kSuccess;
    }
}


void* MirrorJointChain::creator()
{
    return new MirrorJointChain();
}

MSyntax MirrorJointChain::newSyntax()
{
    MSyntax syntax = ChainCommandUtils::createSyntax();
    syntax.addFlag("-ct", "-chainType", MSyntax::kString);

    return syntax;
}

MStatus MirrorJointChain::doIt(const MArgList& args)
{
    MStatus status;

    MArgDatabase database(syntax(), args, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot read command arguments");

    if (!database.isFlagSet("-module") ||
        !database.isFlagSet("-side") ||
        !database.isFlagSet("-chainType"))
    {
        MGlobal::displayError("module, side and chainType are required");
        return MS::kInvalidParameter;
    }

    MString module, sourceSide, chainType;
    database.getFlagArgument("-module", 0, module);
    database.getFlagArgument("-side", 0, sourceSide);
    database.getFlagArgument("-chainType", 0, chainType);
    module.toLowerCase();
    chainType.toLowerCase();
    sourceSide.toUpperCase();

    // Valid parameter check
    if (sourceSide != "L" && sourceSide != "R")
    {
        MGlobal::displayError("Mirror side must be L or R");
        return MS::kInvalidParameter;
    }

    if (chainType != "fk" && chainType != "ik")
    {
        MGlobal::displayError("chainType must be fk or ik");
        return MS::kInvalidParameter;
    }

    const MString targetSide = oppositeSide(sourceSide);

    if (module == "hand")
    {
        status = mirrorTree(module, sourceSide, targetSide, chainType);
    }
    else
    {
        status = mirrorSingleChain(module, sourceSide, targetSide, chainType);
    }
    RETURN_IF_MAYA_FAILED(status, "Cannot mirror joint module");

    MGlobal::displayInfo(module + "_" + chainType + " mirror joint create successfully");
    return MS::kSuccess;
}
