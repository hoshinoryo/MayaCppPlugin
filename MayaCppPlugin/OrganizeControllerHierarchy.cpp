#include "OrganizeControllerHierarchy.h"
#include "BoneChainDefinition.h"
#include "RigModuleRegistry.h"
#include "StatusUtils.h"
#include "FuncUtils.h"

#include <vector>

namespace
{
    //constexpr const char* ROOT_CONTROLLER_GROUP = "M_Root_ctrl_grp";
    constexpr const char* ROOT_CONTROLLER = "M_root_bind_ctrl";

    struct ControllerParenting // controller relationship records
    {
        MString child;  // which is child
        MString parent; // which is parent
    };

    MString fkControllerName(const BoneStructureBase& chain, const BoneBase& bone)
    {
        return chain.prefix() + "_" + bone.label + "_fk_ctrl";
    }

    MString fkControllerGroupName(const BoneStructureBase& chain, const BoneBase& bone)
    {
        return chain.prefix() + "_" + bone.label + "_fk_ctrl_grp";
    }

    MString ikControllerName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ik_ctrl";
    }

    MString ikControllerGroupName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ik_ctrl_grp";
    }

    MString poleControllerName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ik_pole_ctrl";
    }

    MString poleControllerGroupName(const SingleChainDefinition& chain)
    {
        return chain.prefix() + "_ik_pole_ctrl_grp";
    }

    void appendLimbHierarchy(
        const SingleChainDefinition& chain,
        const MString& fkParentController,
        const MString& ikParentController,
        std::vector<ControllerParenting>& result
    )
    {
        result.push_back(
            {
                fkControllerGroupName(chain, chain.bones.front()),
                fkParentController
            }
        );

        result.push_back(
            {
                ikControllerGroupName(chain),
                ikParentController
            }
        );

        result.push_back(
            {
                poleControllerGroupName(chain),
                ikControllerGroupName(chain)
            }
        );
    }

    MStatus buildHierarchyDefinition(std::vector<ControllerParenting>& result)
    {
        MStatus status;

        SingleChainDefinition spine;
        SingleChainDefinition head;
        SingleChainDefinition leftArm;
        SingleChainDefinition rightArm;
        SingleChainDefinition leftLeg;
        SingleChainDefinition rightLeg;

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

        const MString pelvisController = fkControllerName(spine, spine.bones.front());
        const MString chestController = fkControllerName(spine, spine.bones.back());
        const MString leftClavicleController = fkControllerName(leftArm, leftArm.bones.front());
        const MString rightClavicleController = fkControllerName(rightArm, rightArm.bones.front());

        result.push_back(
            {
                fkControllerGroupName(spine, spine.bones.front()),
                ROOT_CONTROLLER
            }
        );
        result.push_back(
            {
                fkControllerGroupName(head, head.bones.front()),
                chestController
            }
        );
        appendLimbHierarchy(leftArm, chestController, leftClavicleController, result);
        appendLimbHierarchy(rightArm, chestController, rightClavicleController, result);
        appendLimbHierarchy(leftLeg, pelvisController, pelvisController, result);
        appendLimbHierarchy(rightLeg, pelvisController, pelvisController, result);

        return MS::kSuccess;
    }

    MStatus parentPreserveWorld(const MString& child, const MString& parent)
    {
        MString command;
        command.format("parent -absolute \"^1s\" \"^2s\"", child, parent);

        return FuncUtils::executeMayaCommand(command, "Cannot parent controller group: " + child);
    }
}

void* OrganizeControllerHierarchy::creator()
{
    return new OrganizeControllerHierarchy();
}

MStatus OrganizeControllerHierarchy::doIt(const MArgList& args)
{
    std::vector<ControllerParenting> hierarchy;

    MStatus status = buildHierarchyDefinition(hierarchy);
    RETURN_IF_MAYA_FAILED(status, "Unable to build controller hierarchy");

    for (const ControllerParenting& item : hierarchy)
    {
        parentPreserveWorld(item.child, item.parent);
    }

    MGlobal::displayInfo("Controller hierarchy organized successfully");

    return MS::kSuccess;
}
