#include "CreateFkController.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "ChainCommandUtils.h"
#include "ControllerShapeUtils.h"


#include <maya/MGlobal.h>
#include <maya/MDagPath.h>

namespace
{
    MString fkControllerName(const BoneStructureBase& chain, const BoneBase& bone)
    {
        return chain.prefix() + "_" + bone.label + "_fk_ctrl";
    }

    MString parentFkControllerName(const TreeBoneDefinition& tree)
    {
        const MString prefix = tree.side.length() == 0 || tree.side == "M"
            ? "M_" + tree.parentModule
            : tree.side + "_" + tree.parentModule;

        return prefix + "_" + tree.parentLabel + "_fk_ctrl";
    }

    // -------------------------------------------------------------------------------------
    // 
    // Validation
    // 
    // -------------------------------------------------------------------------------------
    MStatus validateJointController(const BoneStructureBase& chain, const BoneBase& bone)
    {
        const MString jointName = chain.jointName(bone, "fk");
        const MString controllerName = fkControllerName(chain, bone);
        const MString groupName = ControllerShapeUtils::controllerGroupName(controllerName);

        if (!FuncUtils::objectExists(jointName))
        {
            MGlobal::displayError("Missing Fk joint: " + jointName);
            return MS::kFailure;
        }
        if (FuncUtils::objectExists(controllerName) || FuncUtils::objectExists(groupName))
        {
            MGlobal::displayError("Controller already exists " + controllerName);
            return MS::kFailure;
        }

        return MS::kSuccess;
    }

    MStatus validateSingleChain(const SingleChainDefinition& chain)
    {
        if (chain.bones.empty())
        {
            MGlobal::displayError("FK chain contains no bones");
            return MS::kFailure;
        }

        for (const BoneBase& bone : chain.bones)
        {
            validateJointController(chain, bone);
        }

        return MS::kSuccess;
    }

    MStatus validateTreeChain(const TreeBoneDefinition& tree)
    {
        if (tree.children.empty())
        {
            MGlobal::displayError("Tree chain contains no child chains");
            return MS::kFailure;
        }

        const MString palmJoint = tree.rootJointName("fk");
        if (!FuncUtils::objectExists(palmJoint))
        {
            MGlobal::displayError("Missing parent joint: " + palmJoint);
            return MS::kFailure;
        }

        const MString parentController = parentFkControllerName(tree);
        if (!FuncUtils::objectExists(parentController))
        {
            MStatus status = validateJointController(tree, tree.root);
            RETURN_IF_MAYA_FAILED(status, "Cannot validate palm FK controller");
        }

        for (const SingleChainDefinition& child : tree.children)
        {
            MStatus status = validateSingleChain(child);
            RETURN_IF_MAYA_FAILED(status, "Cannot validate finger Fk chain");
        }

        return MS::kSuccess;
    }
    // -------------------------------------------------------------------------------------



    // -------------------------------------------------------------------------------------
    MStatus createFkControllers(const BoneStructureBase& chain, const BoneBase& bone,
        ControllerShapeUtils::ShapeType shapeType)
    {
        MStatus status;

        const MString jointName = chain.jointName(bone, "fk");
        const MString controllerName = fkControllerName(chain, bone);
        const MString groupName = ControllerShapeUtils::controllerGroupName(controllerName);

        MObject controllerTransform;

        status = ControllerShapeUtils::createController(
            controllerName,
            shapeType,
            bone.controllerRadius,
            controllerTransform
        );
        RETURN_IF_MAYA_FAILED(status, "Unable to create FK controller");

        FuncUtils::matchWorldPositionAndRotation(groupName, jointName); // match position

        MDagPath shapePath;
        FuncUtils::getShapeFromTransform(controllerName, shapePath);
        FuncUtils::setDisplayColor(shapePath.node(), chain.controllerColor); // set color

        return MS::kSuccess;
    }

    MStatus parentControllerGroup(const MString& childController, const MString& parentController)
    {
        const MString childGroup = ControllerShapeUtils::controllerGroupName(childController);

        MString command;
        command.format("parent -absolute \"^1s\" \"^2s\"", childGroup, parentController);
        return FuncUtils::executeMayaCommand(command, "Cannot parent elbow controller");
    }

    MStatus constrainJoint(const MString& controllerName, const MString& jointName)
    {
        MString command;
        command.format("parentConstraint -maintainOffset -name \"^1s\" \"^2s\" \"^3s\"",
            jointName + "_parentConstraint",
            controllerName,
            jointName);
        return FuncUtils::executeMayaCommand(command, "Cannot create FK parent constraint");
    }
    // -------------------------------------------------------------------------------------



    MStatus createSingleChainHierarchy(const SingleChainDefinition& chain, ControllerShapeUtils::ShapeType shapeType)
    {
        MStatus status;

        for (const BoneBase& bone : chain.bones)
        {
            createFkControllers(chain, bone, shapeType);
        }

        for (size_t i = 1; i < chain.bones.size(); i++)
        {
            const MString parentController = fkControllerName(chain, chain.bones[i - 1]);
            const MString childController = fkControllerName(chain, chain.bones[i]);

            parentControllerGroup(childController, parentController);
        }

        for (const BoneBase& bone : chain.bones)
        {
            const MString jointName = chain.jointName(bone, "fk");
            const MString controllerName = fkControllerName(chain, bone);

            constrainJoint(controllerName, jointName);
        }

        return MS::kSuccess;
    }

    MStatus createSingleChainControllers(const SingleChainDefinition& chain)
    {
        MStatus status = createSingleChainHierarchy(chain, ControllerShapeUtils::ShapeType::Circle);
        RETURN_IF_MAYA_FAILED(status, "Cannot create single fk controllers");

        MGlobal::displayInfo(chain.prefix() + " Fk controllers created successfully");
        return MS::kSuccess;
    }

    MStatus createTreeChainControllers(const TreeBoneDefinition& tree)
    {
        MStatus status;
        MString handController = parentFkControllerName(tree);

        if (!FuncUtils::objectExists(handController))
        {
            createFkControllers(tree, tree.root, ControllerShapeUtils::ShapeType::Circle);
            handController = fkControllerName(tree, tree.root);
            constrainJoint(handController, tree.rootJointName("fk"));
        }

        for (const SingleChainDefinition& child : tree.children)
        {
            MStatus status = createSingleChainHierarchy(child, ControllerShapeUtils::ShapeType::Lollipop);
            RETURN_IF_MAYA_FAILED(status, "Cannot create finger fk controllers");

            const MString rootController = fkControllerName(child, child.bones.front());
            parentControllerGroup(rootController, handController);
        }

        MGlobal::displayInfo(tree.prefix() + " Fk controllers created successfully");
        return MS::kSuccess;
    }
}

void* CreateFkController::creator()
{
    return new CreateFkController();
}

MSyntax CreateFkController::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}

MStatus CreateFkController::doIt(const MArgList& args)
{
    MStatus status;
    MArgDatabase database(syntax(), args, &status);

    MString module = "arm";
    if (database.isFlagSet("-module"))
    {
        database.getFlagArgument("-module", 0, module);
    }
    module.toLowerCase();
    m_IsTree = module == "hand";

    // Validation
    if (m_IsTree)
    {
        status = ChainCommandUtils::parseDefinition(syntax(), args, m_Tree);
        RETURN_IF_MAYA_FAILED(status, "Cannot read tree chain definition");

        status = validateTreeChain(m_Tree);
        RETURN_IF_MAYA_FAILED(status, "Cannot validate hand FK chain");
    }
    else
    {
        status = ChainCommandUtils::parseDefinition(syntax(), args, m_Chain);
        RETURN_IF_MAYA_FAILED(status, "Cannot read signle chain definition");

        status = validateSingleChain(m_Chain);
        RETURN_IF_MAYA_FAILED(status, "Cannot validate single FK chain");
    }

    return redoIt();
}

MStatus CreateFkController::redoIt()
{
    if (m_IsTree)
    {
        return createTreeChainControllers(m_Tree);
    }

    return createSingleChainControllers(m_Chain);
}

MStatus CreateFkController::undoIt()
{
    return MStatus();
}

bool CreateFkController::isUndoable() const
{
    return true;
}
