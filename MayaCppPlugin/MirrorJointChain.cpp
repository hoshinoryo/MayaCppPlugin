#include "MirrorJointChain.h"
#include "ChainCommandUtils.h"
#include "StatusUtils.h"
#include "FuncUtils.h"


namespace
{
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

    MString sidePrefix(const MString& module, const MString& side)
    {
        return side + "_" + module;
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

    if (!database.isFlagSet("-module"))
    {
        MGlobal::displayError("module is required");
        return MS::kInvalidParameter;
    }
    if (!database.isFlagSet("-side"))
    {
        MGlobal::displayError("side is required");
        return MS::kInvalidParameter;
    }
    if (!database.isFlagSet("-chainType"))
    {
        MGlobal::displayError("chainType is required");
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
        MGlobal::displayError("side must be L or R");
        return MS::kInvalidParameter;
    }

    if (chainType != "fk" && chainType != "ik")
    {
        MGlobal::displayError("chainType must be fk or ik");
        return MS::kInvalidParameter;
    }

    const MString targetSide = oppositeSide(sourceSide);

    BoneChainDefinition sourceChain, targetChain;

    RigModuleRegistry::getChain(module, sourceSide, sourceChain);
    RigModuleRegistry::getChain(module, targetSide, targetChain);

    if (sourceChain.bones.empty())
    {
        MGlobal::displayError("Source chain has no bones");
        return MS::kFailure;
    }
    if (targetChain.bones.size() != sourceChain.bones.size())
    {
        MGlobal::displayError("Source and target chain definition have different bone counts");
        return MS::kFailure;
    }

    const MString sourceRootJoint = sourceChain.jointName(sourceChain.bones.front(), chainType);
    const MString targetRootJoint = targetChain.jointName(targetChain.bones.front(), chainType);

    if (!FuncUtils::objectExists(sourceRootJoint))
    {
        MGlobal::displayError("Source root joint does not exists: " + sourceRootJoint);
        return MS::kFailure;
    }

    for (const BoneDefinition& targetBone : targetChain.bones)
    {
        const MString targetJoint = targetChain.jointName(targetBone, chainType);

        if (FuncUtils::objectExists(targetJoint))
        {
            MGlobal::displayError("Target joint already exists: " + targetJoint);
            return MS::kFailure;
        }
    }

    // Name replace rule
    const MString sourcePrefix = sidePrefix(module, sourceSide);
    const MString targetPrefix = sidePrefix(module, targetSide);

    // Maya mirror command
    MString command;
    command.format("mirrorJoint -mirrorYZ -mirrorBehavior -searchReplace \"^1s\" \"^2s\" \"^3s\"",
        sourcePrefix,
        targetPrefix,
        sourceRootJoint);

    status = FuncUtils::executeMayaCommand(command, "Failed to create mirror joint");
    if (!status) return status;

    MGlobal::displayInfo("Mirror joint create successfully");

    return MS::kSuccess;
}
