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
    BoneChainDefinition createArmDefinition(const MString& side)
    {
        BoneChainDefinition chain;

        chain.module = "arm";
        chain.side = side;
        chain.guideColor = 14;
        chain.guideCurveColor = 18;
        chain.controllerColor = side == "R" ? 13 : 6;
        
        const double direction = side == "R" ? -1.0 : 1.0;

        chain.bones =
        {
            {
                "shoulder",
                MVector(3.0, 18.0, 0.0),
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

    BoneChainDefinition createLegDefinition(const MString& side)
    {
        BoneChainDefinition chain;

        chain.module = "leg";
        chain.side = side;
        chain.guideColor = 14;
        chain.guideCurveColor = 18;
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

    BoneChainDefinition createSpineDefinition()
    {
        BoneChainDefinition chain;

        chain.module = "spine";
        chain.side = "M";
        chain.guideColor = 14;
        chain.guideCurveColor = 18;
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

    BoneChainDefinition createHeadDefinition()
    {
        BoneChainDefinition chain;

        chain.module = "head";
        chain.side = "M";
        chain.guideColor = 14;
        chain.guideCurveColor = 18;
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
    BoneChainDefinition& result
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

    MGlobal::displayError("Unspported rig module: " + module);
    return MS::kFailure;
}
