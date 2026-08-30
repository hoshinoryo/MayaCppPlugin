///----------------------------------------------------------------------------------------
///
/// [Rig module management]
/// New module will be registered here.
/// 
///----------------------------------------------------------------------------------------

#include "RigModuleRegistry.h"

#include <maya/MGlobal.h>


namespace
{
    // ---------------------------
    // Full body bone setting
    // ---------------------------
    struct BoneSpec
    {
        const char* label;
        MVector     position;
        double      controllerRadius;

        JointBuildMode jointBuildMode;

        const char* parentModule;
        const char* parentJoint;
        const char* parentSide;
    };

    struct ChainSpec
    {
        const char* module;
        const char* chainLabel;

        std::vector<BoneSpec> bones;
    };

    struct ChildChainSpec
    {
        const char* chainLabel;
        std::vector<BoneSpec> bones;
        // module is from tree specification
    };

    struct TreeSpec
    {
        const char* module;
        BoneSpec root;
        std::vector<ChildChainSpec> children;
    };

    // ----------------------------
    // Skeleton specification
    // ----------------------------
    const ChainSpec SPINE_SPEC = {
        "spine",
        "spine",
        {
            { "pelvis",   MVector(0.0, 14.0, 0.0), 2.5, JointBuildMode::FkOnly, "", "", "" },
            { "spine_01", MVector(0.0, 17.0, 0.4), 1.9, JointBuildMode::FkOnly, "spine", "pelvis", "M" },
            { "spine_02", MVector(0.0, 19.0, 0.4), 1.5, JointBuildMode::FkOnly, "spine", "spine_01", "M" },
            { "chest",    MVector(0.0, 22.0, 0.0), 1.8, JointBuildMode::FkOnly, "spine", "spine_02", "M" }
        }
    };

    const ChainSpec HEAD_SPEC = {
        "head",
        "head",
        {
            { "neck", MVector(0.0, 23.0, 0.0), 1.5, JointBuildMode::FkOnly, "spine", "chest", "M" },
            { "head", MVector(0.0, 26.0, 0.0), 1.8, JointBuildMode::FkOnly, "head", "neck", "M" }
        }
    };

    // With position direction
    const ChainSpec ARM_SPEC = {
        "arm",
        "arm",
        {
            { "clavicle", MVector(2.0, 22.0,  0.0),  1.6, JointBuildMode::FkOnly, "spine", "chest", "M" },
            { "shoulder", MVector(3.0, 22.0,  0.0),  1.6, JointBuildMode::FkAndIk, "arm", "clavicle", "" },
            { "elbow",    MVector(8.0, 22.0, -0.5),  1.4, JointBuildMode::FkAndIk, "arm", "shoulder", "" },
            { "wrist",    MVector(12.0, 22.0,  0.0), 1.2, JointBuildMode::FkAndIk, "arm", "elbow", "" }
        }
    };

    const ChainSpec LEG_SPEC = {
        "leg",
        "leg",
        {
            { "thigh", MVector(1.5, 14.0, 0.0), 1.6, JointBuildMode::FkAndIk, "spine", "pelvis", "M" },
            { "knee",  MVector(1.5, 7.0, 0.5),  1.3, JointBuildMode::FkAndIk, "leg", "thigh", "" },
            { "ankle", MVector(1.5, 1.0, 0.0),  1.1, JointBuildMode::FkAndIk, "leg", "knee", "" },
            { "ball",  MVector(1.5, 0.5, 2.0),  1.0, JointBuildMode::FkOnly, "leg", "ankle", "" },
            { "toe",   MVector(1.5, 0.5, 3.5),  0.8, JointBuildMode::FkOnly, "leg", "ball", "" }
        }
    };

    const TreeSpec HAND_SPEC = {
        "hand",
        { "palm", MVector(13.0, 22.0, 0.0), 1.2, JointBuildMode::FkOnly, "arm", "wrist", "" },
        {
            {
                "thumb",
                {
                    { "thumb_01", MVector(13.6, 22.0, 0.4), 0.5, JointBuildMode::FkOnly, "hand", "palm", "" },
                    { "thumb_02", MVector(14.1, 22.0, 0.5), 0.5, JointBuildMode::FkOnly, "hand", "thumb_01", "" },
                    { "thumb_03", MVector(14.6, 22.0, 0.5), 0.5, JointBuildMode::FkOnly, "hand", "thumb_02", "" },
                }
            },
            {
                "index",
                {
                    { "index_01", MVector(14.2, 22.0, 0.2), 0.5, JointBuildMode::FkOnly, "hand", "palm", "" },
                    { "index_02", MVector(14.8, 22.0, 0.2), 0.5, JointBuildMode::FkOnly, "hand", "index_01", "" },
                    { "index_03", MVector(15.2, 22.0, 0.2), 0.5, JointBuildMode::FkOnly, "hand", "index_02", "" },
                }
            },
            {
                "mid",
                {
                    { "mid_01", MVector(14.5, 22.0, 0.0), 0.5, JointBuildMode::FkOnly, "hand", "palm", "" },
                    { "mid_02", MVector(15.1, 22.0, 0.0), 0.5, JointBuildMode::FkOnly, "hand", "mid_01", "" },
                    { "mid_03", MVector(15.5, 22.0, 0.0), 0.5, JointBuildMode::FkOnly, "hand", "mid_02", "" },
                }
            },
            {
                "ring",
                {
                    { "ring_01", MVector(14.4, 22.0, -0.2), 0.5, JointBuildMode::FkOnly, "hand", "palm", "" },
                    { "ring_02", MVector(15.0, 22.0, -0.2), 0.5, JointBuildMode::FkOnly, "hand", "ring_01", "" },
                    { "ring_03", MVector(15.4, 22.0, -0.2), 0.5, JointBuildMode::FkOnly, "hand", "ring_02", "" },
                }
            },
            {
                "pinky",
                {
                    { "pinky_01", MVector(14.2, 22.0, -0.4), 0.5, JointBuildMode::FkOnly, "hand", "palm", "" },
                    { "pinky_02", MVector(14.8, 22.0, -0.4), 0.5, JointBuildMode::FkOnly, "hand", "pinky_01", "" },
                    { "pinky_03", MVector(15.2, 22.0, -0.4), 0.5, JointBuildMode::FkOnly, "hand", "pinky_02", "" },
                }
            }
        }
    };

    // Registry
    const std::vector<const ChainSpec*> CHAIN_SPECS =
    {
        &SPINE_SPEC,
        &HEAD_SPEC,
        &ARM_SPEC,
        &LEG_SPEC
    };

    const std::vector<const TreeSpec*> TREE_SPECS =
    {
        &HAND_SPEC
    };

    const ChainSpec* findChainSpec(const MString& module)
    {
        for (const ChainSpec* spec : CHAIN_SPECS)
        {
            if (module == spec->module)
            {
                return spec;
            }
        }
        return nullptr;
    }

    const TreeSpec* findTreeSpec(const MString& module)
    {
        for (const TreeSpec* spec : TREE_SPECS)
        {
            if (module == spec->module)
            {
                return spec;
            }
        }
        return nullptr;
    }

    // ----------------------------
    // Common resolving
    // ----------------------------
    double sideDirection(const MString& side)
    {
        if (side == "R") return -1.0;
        return 1.0;
    }

    MVector resolvePosition(const BoneSpec& spec, const MString& side)
    {
        MVector result = spec.position;

        if (side == "L" || side == "R")
        {
            result.x *= sideDirection(side);
        }
        return result;
    }

    short controllerColor(const MString& side)
    {
        if (side == "R")
        {
            return 13;
        }
        if (side == "M")
        {
            return 17;
        }

        return 6;
    }

    // Turn BoneSpec to BoneBase
    BoneBase buildBoneDefinition(const BoneSpec& spec, const MString& side)
    {
        BoneBase bone;
        
        bone.label = spec.label;
        bone.position = resolvePosition(spec, side);
        bone.controllerRadius = spec.controllerRadius;
        bone.jointBuildMode = spec.jointBuildMode;
        bone.parent = { spec.parentModule, spec.parentJoint, spec.parentSide };

        return bone;
    }

    // ------------------------------
    // module
    //  |
    // findChainSpec() / findTreeSpec()
    //  |
    // createChainDefinition() / createTreeDefinition()
    // ------------------------------
    MStatus createChainDefinition(const ChainSpec& spec, const MString& side, SingleChainDefinition& result)
    {
        result = SingleChainDefinition();
        result.module = spec.module;
        result.side = side;
        result.chainLabel = spec.chainLabel;
        result.controllerColor = controllerColor(side);

        result.bones.clear();
        result.bones.reserve(spec.bones.size());

        for (const BoneSpec& boneSpec : spec.bones)
        {
            result.bones.push_back(buildBoneDefinition(boneSpec, side));
        }

        if (result.bones.empty())
        {
            MGlobal::displayError("Chain contains no bones: " + result.module);
            return MS::kFailure;
        }

        return MS::kSuccess;
    }

    MStatus createChildChainDefinition(
        const TreeBoneDefinition& tree,
        const ChildChainSpec& childSpec,
        SingleChainDefinition& result
    )
    {
        result = SingleChainDefinition();
        result.module          = tree.module;
        result.side            = tree.side;
        result.chainLabel      = childSpec.chainLabel;
        result.guideColor      = tree.guideColor;
        result.guideCurveColor = tree.guideCurveColor;
        result.controllerColor = tree.controllerColor;
        result.parentGuide     = tree.rootGuideName();
        result.parentJoint     = tree.rootJointName("fk");

        result.bones.clear();
        result.bones.reserve(childSpec.bones.size());

        for (const BoneSpec& boneSpec : childSpec.bones)
        {
            result.bones.push_back(buildBoneDefinition(boneSpec, tree.side));
        }

        if (result.bones.empty())
        {
            MGlobal::displayError("Tree chain contains no bones: " + result.chainLabel);
            return MS::kFailure;
        }

        return MS::kSuccess;
    }

    MStatus createTreeDefinition(const TreeSpec& spec, const MString& side, TreeBoneDefinition& result)
    {
        result = TreeBoneDefinition();
        result.module = spec.module;
        result.side = side;
        result.controllerColor = controllerColor(side);

        result.root = buildBoneDefinition(spec.root, side);
        if (!result.root.parent.isValid())
        {
            MGlobal::displayError("Tree root requires a parent bone: " + result.module);
            return MS::kFailure;
        }

        result.parentModule = result.root.parent.module;
        result.parentLabel = result.root.parent.label;

        result.children.clear();
        result.children.reserve(spec.children.size());

        for (const ChildChainSpec& childSpec : spec.children)
        {
            SingleChainDefinition child;
            
            MStatus status = createChildChainDefinition(result, childSpec, child);
            if (!status) return status;

            result.children.push_back(child);
        }

        if (result.children.empty())
        {
            MGlobal::displayError("Tree contains no child chains: " + result.module);
            return MS::kFailure;
        }

        return MS::kSuccess;
    }
}


// --------------------------------------------------
// Public registry
// --------------------------------------------------
MStatus RigModuleRegistry::getChain(
    const MString& module,
    const MString& side,
    SingleChainDefinition& result
)
{
    const ChainSpec* spec = findChainSpec(module);

    if (spec == nullptr)
    {
        MGlobal::displayError("Unsupported chain module: " + module);
        return MS::kFailure;
    }

    return createChainDefinition(*spec, side, result);
}

MStatus RigModuleRegistry::getTree(const MString& module, const MString& side, TreeBoneDefinition& result)
{
    const TreeSpec* spec = findTreeSpec(module);

    if (spec == nullptr)
    {
        MGlobal::displayError("Unsupported chain module: " + module);
        return MS::kFailure;
    }

    return createTreeDefinition(*spec, side, result);
}
