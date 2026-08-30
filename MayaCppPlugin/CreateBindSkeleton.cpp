#include "CreateBindSkeleton.h"
#include "BoneChainDefinition.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "RigModuleRegistry.h"
#include "ControllerShapeUtils.h"

#include <vector>
#include <maya/MGlobal.h>
#include <maya/MFnIkJoint.h>
#include <maya/MQuaternion.h>
#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MArgDatabase.h>
#include <maya/MSelectionList.h>
#include <maya/MPlug.h>


namespace
{
    constexpr const char* ROOT_BIND_JOINT = "M_root_bind_jnt";
    constexpr const char* ROOT_BIND_CONTROLLER = "M_root_bind_ctrl";
    constexpr const char* ROOT_BIND_CONTROLLER_GROUP = "M_root_bind_ctrl_grp";

    struct BindBone
    {
        MString bindJoint;
        MString parentJoint;

        std::vector<MString> drivers;
    };

    MString constraintName(const MString& bindJoint)
    {
        return bindJoint + "_parentConstraint";
    }

    MString registeredJointName(const MString& module, const MString& side, const MString& bone, const MString& chainType)
    {
        const MString prefix = side.length() == 0 || side == "M"
            ? "M_" + module
            : side + "_" + module;

        return prefix + "_" + bone + "_" + chainType + "_jnt";
    }

    MString bindParentName(const BoneStructureBase& owner, const BoneBase& bone)
    {
        if (!bone.parent.isValid())
        {
            return ROOT_BIND_JOINT;
        }
        
        const MString parentSide = bone.parent.side.length() > 0
            ? bone.parent.side
            : owner.side;

        return registeredJointName(bone.parent.module, parentSide, bone.parent.label, "bind");
    }

    MStatus appendBone(
        const BoneStructureBase& chain,
        const BoneBase& bone,
        std::vector<BindBone>& result
    )
    {
        BindBone bindBone;

        bindBone.bindJoint = chain.jointName(bone, "bind");
        bindBone.parentJoint = bindParentName(chain, bone);

        const MString fkJoint = chain.jointName(bone, "fk");
        const MString ikJoint = chain.jointName(bone, "ik");

        if (bone.buildsJointType("fk") && FuncUtils::objectExists(fkJoint))
        {
            bindBone.drivers.push_back(fkJoint);
        }
        if (bone.buildsJointType("ik") && FuncUtils::objectExists(ikJoint))
        {
            bindBone.drivers.push_back(ikJoint);
        }

        if (!bindBone.drivers.empty())
        {
            result.push_back(bindBone);
        }

        return MS::kSuccess;
    }

    MStatus appendSingleChain(
        const SingleChainDefinition& chain,
        std::vector<BindBone>& result
    )
    {
        for (const BoneBase& bone : chain.bones)
        {
            MStatus status = appendBone(chain, bone, result);
            if (!status) return status;
        }

        return MS::kSuccess;
    }

    MStatus appendTree(
        const TreeBoneDefinition& tree,
        std::vector<BindBone>& result
    )
    {
        MStatus status = appendBone(tree, tree.root, result);
        RETURN_IF_MAYA_FAILED(status, "Cannot append tree root bind bone");

        for (const SingleChainDefinition& chain : tree.children)
        {
            appendSingleChain(chain, result);
        }

        return MS::kSuccess;
    }

    MStatus buildBindDefinition(std::vector<BindBone>& result)
    {
        struct ChainRequest
        {
            const char* module;
            const char* side;
        };

        const ChainRequest chainRequests[] = {
            { "spine", "M" },
            { "head", "M" },
            { "arm", "L" },
            { "arm", "R" },
            { "leg", "L" },
            { "leg", "R" }
        };
        const ChainRequest treeRequests[] = {
            { "hand", "L" },
            { "hand", "R" }
        };

        for (const ChainRequest& request : chainRequests)
        {
            SingleChainDefinition chain;
            MStatus status = RigModuleRegistry::getChain(request.module, request.side, chain);
            RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");

            appendSingleChain(chain, result);
        }
        for (const ChainRequest& request : treeRequests)
        {
            TreeBoneDefinition tree;
            MStatus status = RigModuleRegistry::getTree(request.module, request.side, tree);
            RETURN_IF_MAYA_FAILED(status, "Cannot read chain definition");

            appendTree(tree, result);
        }

        return MS::kSuccess;
    }

    // -------------------------------------------------------------------------------------
    // Root joint and controller
    // -------------------------------------------------------------------------------------
    MStatus createRootJoint(MObject& rootObject)
    {
        MStatus status;

        MFnIkJoint rootFn; // Create joints
        rootObject = rootFn.create(MObject::kNullObj, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create root joint");

        rootFn.setName(ROOT_BIND_JOINT, false);

        // Set position
        rootFn.setTranslation(MVector(0.0, 0.0, 0.0), MSpace::kTransform);
        rootFn.setOrientation(MQuaternion());

        return MS::kSuccess;
    }

    MStatus createRootController()
    {
        MStatus status;
        MObject controllerTransform;

        status = ControllerShapeUtils::createController(
            ROOT_BIND_CONTROLLER,
            ControllerShapeUtils::ShapeType::Circle,
            5.0,
            controllerTransform);
        RETURN_IF_MAYA_FAILED(status, "Failed to create root controller");

        MString command;
        command.format("rotate -relative -objectSpace 0 0 90 \"^1s.cv[*]\";",
            ROOT_BIND_CONTROLLER);
        FuncUtils::executeMayaCommand(command, "Cannot rotate root controller shape");

        MDagPath shapePath;
        FuncUtils::getShapeFromTransform(ROOT_BIND_CONTROLLER, shapePath);
        FuncUtils::setDisplayColor(shapePath.node(), 20);
        FuncUtils::matchWorldPositionAndRotation(ROOT_BIND_CONTROLLER_GROUP, ROOT_BIND_JOINT);

        command.format("parentConstraint -name \"^1s\" \"^2s\" \"^3s\";",
            MString(ROOT_BIND_JOINT) + "_parentConstraint",
            ROOT_BIND_CONTROLLER,
            ROOT_BIND_JOINT);

        return FuncUtils::executeMayaCommand(command, "Cannot constrain root bind joint");
    }

    MStatus createBindJoint(const BindBone& bone)
    {
        MStatus status;

        MObject parentObject = MObject::kNullObj;
        MMatrix parentWorldMatrix = MMatrix::identity;
        MString parentJoint;
        
        if (FuncUtils::objectExists(bone.parentJoint))
        {
            parentJoint = bone.parentJoint;
        }
        else if (FuncUtils::objectExists(ROOT_BIND_JOINT))
        {
            parentJoint = ROOT_BIND_JOINT;
        }

        if (parentJoint.length() > 0)
        {
            MDagPath parentPath;
            FuncUtils::getDagPath(parentJoint, parentPath);

            parentObject = parentPath.node();
            parentWorldMatrix = parentPath.inclusiveMatrix(&status);
            RETURN_IF_MAYA_FAILED(status, "Cannot read bind parent world matrix");
        }

        MMatrix worldMatrix;
        FuncUtils::getWorldMatrix(bone.drivers.front(), worldMatrix); // world matrix get from driver

        const MMatrix localMatrix = worldMatrix * parentWorldMatrix.inverse();
        const MTransformationMatrix localTransform(localMatrix);

        const MVector localTranslation  = localTransform.getTranslation(MSpace::kTransform);
        const MQuaternion localRotation = localTransform.rotation();

        MFnIkJoint jointFn;
        MObject jointObject = jointFn.create(parentObject, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create bind joint: " + bone.bindJoint);

        jointFn.setName(bone.bindJoint, false);

        // Set position
        jointFn.setTranslation(localTranslation, MSpace::kTransform);
        jointFn.setOrientation(localRotation);

        return MS::kSuccess;
    }

    MStatus createParentConstraint(const BindBone& bone)
    {
        MString command = "parentConstraint -name \"" + constraintName(bone.bindJoint) + "\"";

        for (const MString& driver : bone.drivers)
        {
            command += " \"" + driver + "\"";
        }

        command += " \"" + bone.bindJoint + "\"";

        return FuncUtils::executeMayaCommand(command, "Cannot constrain bind joint: " + bone.bindJoint);
    }

    MStatus hideAndLockJoint(const MString& jointName)
    {
        MStatus status;
        MSelectionList selection;

        selection.add(jointName);

        MObject jointObject;
        status = selection.getDependNode(0, jointObject);
        RETURN_IF_MAYA_FAILED(status, "Cannot read driver joint: " + jointName);

        MFnDependencyNode jointFn(jointObject);

        MPlug visibilityPlug = jointFn.findPlug("visibility", true,&status);
        RETURN_IF_MAYA_FAILED(status, "Cannot find joint visibility : " + jointName);

        if (visibilityPlug.isLocked())
        {
            visibilityPlug.setLocked(false);
        }

        visibilityPlug.setBool(false);
        visibilityPlug.setLocked(true);

        return MS::kSuccess;
    }

    MStatus hideAndLockJoints(const std::vector<BindBone>& bones)
    {
        for (const BindBone& bone : bones)
        {
            for (const MString& driver : bone.drivers)
            {
                MStatus status = hideAndLockJoint(driver);
                RETURN_IF_MAYA_FAILED(status, "Cannot hide FK/IK driver joint");
            }
        }

        return MS::kSuccess;
    }
}


void* CreateBindSkeleton::creator()
{
    return new CreateBindSkeleton();
}

MSyntax CreateBindSkeleton::newSyntax()
{
    MSyntax syntax;
    syntax.addFlag("-cr", "-createRoot", MSyntax::kBoolean);

    return syntax;
}

MStatus CreateBindSkeleton::doIt(const MArgList& args)
{
    MStatus status;
    MArgDatabase database(syntax(), args, &status);

    bool createRoot = true;
    if (database.isFlagSet("-createRoot"))
    {
        database.getFlagArgument("-createRoot", 0, createRoot);
    }

    std::vector<BindBone> bones;
    status = buildBindDefinition(bones);
    RETURN_IF_MAYA_FAILED(status, "Failed to build bind skeleton definition");

    if (bones.empty())
    {
        MGlobal::displayWarning("No fk or ik joints found for bind skeleton");
    }
    if (createRoot)
    {
        if (!FuncUtils::objectExists(ROOT_BIND_JOINT))
        {
            MObject rootObject;
            createRootJoint(rootObject);
        }
        if (!FuncUtils::objectExists(ROOT_BIND_CONTROLLER))
        {
            createRootController();
        }
    }
    
    for (const BindBone& bone : bones)
    {
        if (FuncUtils::objectExists(bone.bindJoint)) continue;

        createBindJoint(bone);
    }

    for (const BindBone& bone : bones)
    {
        if (FuncUtils::objectExists(constraintName(bone.bindJoint))) continue;

        createParentConstraint(bone);
    }

    status = hideAndLockJoints(bones);
    RETURN_IF_MAYA_FAILED(status, "Cannot hide FK/IK driver joints");

    MGlobal::displayInfo("Complete bind skeleton created successfully");

    return MStatus();
}
