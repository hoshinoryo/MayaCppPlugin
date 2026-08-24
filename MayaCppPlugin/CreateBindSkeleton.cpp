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

    MStatus appendSingleChain(
        const SingleChainDefinition& chain,
        const MString& parentBindJoint,
        bool useIk,
        std::vector<BindBone>& result
    )
    {
        if (chain.bones.empty())
        {
            MGlobal::displayError("Module contains no bones: " + chain.prefix());
            return MS::kFailure;
        }

        MString currentParent = parentBindJoint;

        for (const BoneBase& bone : chain.bones)
        {
            BindBone bindBone;

            bindBone.bindJoint = chain.jointName(bone, "bind");
            bindBone.parentJoint = currentParent;
            bindBone.drivers.push_back(chain.jointName(bone, "fk"));

            if (useIk)
            {
                bindBone.drivers.push_back(chain.jointName(bone, "ik"));
            }

            result.push_back(bindBone);
            currentParent = bindBone.bindJoint;
        }

        return MS::kSuccess;
    }

    MStatus appendTreeChain(
        const TreeBoneDefinition& tree,
        const MString& parentBindJoint,
        std::vector<BindBone>& result
    )
    {
        BindBone root;

        root.bindJoint = tree.rootJointName("bind");
        root.parentJoint = parentBindJoint;
        root.drivers.push_back(tree.rootJointName("fk"));

        result.push_back(root);

        for (const SingleChainDefinition& chain : tree.children)
        {
            MStatus status = appendSingleChain(chain, root.bindJoint, false, result);
            RETURN_IF_MAYA_FAILED(status, "Cannot append child bind chain");
        }

        return MS::kSuccess;
    }

    MStatus buildBindDefinition(std::vector<BindBone>& result)
    {
        MStatus status;

        SingleChainDefinition spine;
        SingleChainDefinition head;
        SingleChainDefinition leftArm;
        SingleChainDefinition rightArm;
        SingleChainDefinition leftLeg;
        SingleChainDefinition rightLeg;

        TreeBoneDefinition leftHand;
        TreeBoneDefinition rightHand;

        status = RigModuleRegistry::getChain("spine", "M", spine);
        RETURN_IF_MAYA_FAILED(status, "Cannot read spine definition");
        status = RigModuleRegistry::getChain("head", "M", head);
        RETURN_IF_MAYA_FAILED(status, "Cannot read head definition");
        status = RigModuleRegistry::getChain("arm", "L", leftArm);
        RETURN_IF_MAYA_FAILED(status, "Cannot read left arm definition");
        status = RigModuleRegistry::getChain("arm", "R", rightArm);
        RETURN_IF_MAYA_FAILED(status, "Cannot read right arm definition");
        status = RigModuleRegistry::getChain("leg", "L", leftLeg);
        RETURN_IF_MAYA_FAILED(status, "Cannot read left leg definition");
        status = RigModuleRegistry::getChain("leg", "R", rightLeg);
        RETURN_IF_MAYA_FAILED(status, "Cannot read right leg definition");
        status = RigModuleRegistry::getTree("hand", "L", leftHand);
        RETURN_IF_MAYA_FAILED(status, "Cannot read left hand definition");
        status = RigModuleRegistry::getTree("hand", "R", rightHand);
        RETURN_IF_MAYA_FAILED(status, "Cannot read right hand definition");

        appendSingleChain(spine, ROOT_BIND_JOINT, false, result);

        const MString pelvisBind = spine.jointName(spine.bones.front(), "bind");
        const MString chestBind = spine.jointName(spine.bones.back(), "bind");

        appendSingleChain(head, chestBind, false, result);
        appendSingleChain(leftArm, chestBind, true, result);
        appendSingleChain(rightArm, chestBind, true, result);
        appendSingleChain(leftLeg, pelvisBind, true, result);
        appendSingleChain(rightLeg, pelvisBind, true, result);

        const MString leftWristBind = leftArm.jointName(leftArm.bones.back(), "bind");
        const MString rightWristBind = rightArm.jointName(rightArm.bones.back(), "bind");

        appendTreeChain(leftHand, leftWristBind, result);
        appendTreeChain(rightHand, rightWristBind, result);

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

        MDagPath parentPath;
        FuncUtils::getDagPath(bone.parentJoint, parentPath);

        MMatrix worldMatrix;
        FuncUtils::getWorldMatrix(bone.drivers.front(), worldMatrix); // world matrix get from driver

        const MMatrix parentWorldMatrix = parentPath.inclusiveMatrix(&status);
        RETURN_IF_MAYA_FAILED(status, "Cannot read bind parent world matrix");

        const MMatrix localMatrix = worldMatrix * parentWorldMatrix.inverse();

        MTransformationMatrix localTransform(localMatrix);

        const MVector localTranslation = localTransform.getTranslation(MSpace::kTransform);
        const MQuaternion localRotation = localTransform.rotation();

        MFnIkJoint jointFn;

        MObject jointObject = jointFn.create(parentPath.node(), &status);
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
}


void* CreateBindSkeleton::creator()
{
    return new CreateBindSkeleton();
}

MStatus CreateBindSkeleton::doIt(const MArgList& args)
{
    MStatus status;

    std::vector<BindBone> bones;
    status = buildBindDefinition(bones);
    RETURN_IF_MAYA_FAILED(status, "Failed to build bind skeleton definition");

    MObject rootObject;
    createRootJoint(rootObject);
    createRootController();

    for (const BindBone& bone : bones)
    {
        createBindJoint(bone);
    }

    for (const BindBone& bone : bones)
    {
        createParentConstraint(bone);
    }

    MGlobal::displayInfo("Complete bind skeleton created successfully");

    return MStatus();
}
