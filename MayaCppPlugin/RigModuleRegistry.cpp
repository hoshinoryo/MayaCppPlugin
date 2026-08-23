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
    SingleChainDefinition createArmDefinition(const MString& side)
    {
        SingleChainDefinition chain;

        chain.module = "arm";
        chain.side = side;
        //chain.guideColor = 14;
        //chain.guideCurveColor = 18;
        chain.controllerColor = side == "R" ? 13 : 6;
        
        const double direction = side == "R" ? -1.0 : 1.0;

        chain.bones =
        {
            {
                "clavicle",
                MVector(2.0 * direction, 18.0, 0.0),
                1.6
            },
            {
                "shoulder",
                MVector(3.0 * direction, 18.0, 0.0),
                1.6
            },
            {
                "elbow",
                MVector(8.0 * direction, 18.0, -0.5),
                1.4
            },
            {
                "wrist",
                MVector(12.0 * direction, 18.0, 0.0),
                1.2
            }
        };

        return chain;
    }

    SingleChainDefinition createFingerChain(
        const TreeBoneDefinition& hand,
        const MString& fingerName,
        const std::vector<BoneBase> bones
    )
    {
        SingleChainDefinition chain;

        chain.module = hand.module;
        chain.side = hand.side;
        chain.chainLabel = fingerName;
        chain.controllerColor = hand.controllerColor;

        chain.parentGuide = hand.rootGuideName();
        chain.bones = bones;

        return chain;
    }

    TreeBoneDefinition createHandDefinition(const MString& side)
    {
        TreeBoneDefinition handChain;

        handChain.module = "hand";
        handChain.side = side;
        handChain.controllerColor = side == "R" ? 13 : 6;

        handChain.parentModule = "arm";
        handChain.parentBone = "wrist";

        const double direction = side == "R" ? -1.0 : 1.0;

        handChain.root =
        {
            "palm",
            MVector(13.0 * direction, 18.0, 0.0),
            1.2
        };
        handChain.children =
        {
            createFingerChain(
                handChain,
                "thumb",
                {
                    { "thumb_01", MVector(13.4 * direction, 18.0, 0.4), 0.5 },
                    { "thumb_02", MVector(14.2 * direction, 18.0, 0.5), 0.5 },
                    { "thumb_03", MVector(14.8 * direction, 18.0, 0.5), 0.5 },
                }
            ),
            createFingerChain(
                handChain,
                "index",
                {
                    { "index_01", MVector(14.2 * direction, 18.0, 0.2), 0.5 },
                    { "index_02", MVector(14.8 * direction, 18.0, 0.2), 0.5 },
                    { "index_03", MVector(15.2 * direction, 18.0, 0.2), 0.5 },
                }
            ),
            createFingerChain(
                handChain,
                "mid",
                {
                    { "mid_01", MVector(14.2 * direction, 18.0, 0.0), 0.5 },
                    { "mid_02", MVector(14.8 * direction, 18.0, 0.0), 0.5 },
                    { "mid_03", MVector(15.2 * direction, 18.0, 0.0), 0.5 },
                }
            ),
            createFingerChain(
                handChain,
                "ring",
                {
                    { "ring_01", MVector(14.2 * direction, 18.0, -0.2), 0.5 },
                    { "ring_02", MVector(14.8 * direction, 18.0, -0.2), 0.5 },
                    { "ring_03", MVector(15.2 * direction, 18.0, -0.2), 0.5 },
                }
            ),
            createFingerChain(
                handChain,
                "pinky",
                {
                    { "pinky_01", MVector(14.2 * direction, 18.0, -0.4), 0.5 },
                    { "pinky_02", MVector(14.8 * direction, 18.0, -0.4), 0.5 },
                    { "pinky_03", MVector(15.2 * direction, 18.0, -0.4), 0.5 },
                }
            )
        };

        return handChain;
    }

    SingleChainDefinition createLegDefinition(const MString& side)
    {
        SingleChainDefinition chain;

        chain.module = "leg";
        chain.side = side;
        //chain.guideColor = 14;
        //chain.guideCurveColor = 18;
        chain.controllerColor = side == "R" ? 13 : 6;

        const double direction = side == "R" ? -1.0 : 1.0;

        chain.bones =
        {
            {
                "thigh",
                MVector(1.5 * direction, 10.0, 0.0),
                1.6
            },
            {
                "knee",
                MVector(1.5 * direction, 5.5, 0.5),
                1.3
            },
            {
                "ankle",
                MVector(1.5 * direction, 1.0, 0.0),
                1.1
            },
            {
                "ball",
                MVector(1.5 * direction, 0.5, 2.0),
                1.0
            },
            {
                "toe",
                MVector(1.5 * direction, 0.5, 3.5),
                0.8
            }
        };

        return chain;
    }

    SingleChainDefinition createSpineDefinition()
    {
        SingleChainDefinition chain;

        chain.module = "spine";
        chain.side = "M";
        //chain.guideColor = 14;
        //chain.guideCurveColor = 18;
        chain.controllerColor = 17;

        chain.bones =
        {
            {"pelvis",   MVector(0.0, 10.0, 0.0), 2.5},
            {"spine_01", MVector(0.0, 13.0, 0.4), 1.9},
            {"spine_02", MVector(0.0, 15.0, 0.4), 1.5},
            {"chest",    MVector(0.0, 18.0, 0.0), 1.8}
        };

        return chain;
    }

    SingleChainDefinition createHeadDefinition()
    {
        SingleChainDefinition chain;

        chain.module = "head";
        chain.side = "M";
        //chain.guideColor = 14;
        //chain.guideCurveColor = 18;
        chain.controllerColor = 17;

        chain.bones =
        {
            {"neck",   MVector(0.0, 19.0, 0.0), 1.5},
            {"head",   MVector(0.0, 22.0, 0.0), 1.8}
        };

        return chain;
    }
}


MStatus RigModuleRegistry::getChain(
    const MString& module,
    const MString& side,
    SingleChainDefinition& result
)
{
    if (module == "arm")
    {
        result = createArmDefinition(side);
        return MS::kSuccess;
    }
    if (module == "leg")
    {
        result = createLegDefinition(side);
        return MS::kSuccess;
    }
    if (module == "spine")
    {
        result = createSpineDefinition();
        return MS::kSuccess;
    }
    if (module == "head")
    {
        result = createHeadDefinition();
        return MS::kSuccess;
    }

    MGlobal::displayError("Unsupported rig module: " + module);
    return MS::kFailure;
}

MStatus RigModuleRegistry::getTree(const MString& module, const MString& side, TreeBoneDefinition& result)
{
    if (module == "hand")
    {
        result = createHandDefinition(side);
        return MS::kSuccess;
    }

    MGlobal::displayError("Unsupported rig module: " + module);
    return MS::kFailure;
}
