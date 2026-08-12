#include "PreBuildBoneChain.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "ChainCommandUtils.h"
#include "BoneChainDefinition.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MFnTransform.h>
#include <maya/MDGModifier.h>
#include <maya/MGlobal.h>
#include <maya/MVector.h>
#include <maya/MPointArray.h>
#include <maya/MFnNurbsCurve.h>


namespace
{
    MStatus createGuideLocator(
        const MString& transformName,
        const MVector& worldPosition,
        short colorIndex,
        MObject& locatorTransform,
        MObject& locatorShape
    )
    {
        MStatus status;
        MFnTransform transformFn; // transform node for locator

        locatorTransform = transformFn.create(MObject::kNullObj, &status);
        RETURN_IF_MAYA_FAILED(status, "Failed to create locator transform node");

        transformFn.setName(transformName, false);
        transformFn.setTranslation(worldPosition, MSpace::kTransform);

        MFnDagNode locatorShapeFn;
        locatorShape = locatorShapeFn.create("locator", locatorTransform);
        locatorShapeFn.setName(transformName + "Shape", false);

        status = FuncUtils::setLocatorSize(locatorShape, 0.65);
        RETURN_IF_MAYA_FAILED(status, "Cannot set locator size");

        return FuncUtils::setDisplayColor(locatorShape, colorIndex);
    }

    MStatus connectLocatorToCurveCV(
        const MObject& locatorShape,
        const MObject& curveShape,
        unsigned int cvIndex,
        MDGModifier& dgModifier
    )
    {
        MStatus status;
        MFnDependencyNode locatorFn(locatorShape, &status);
        MFnDependencyNode curveFn(curveShape, &status);

        MPlug worldPositionArray = locatorFn.findPlug("worldPosition", true);
        MPlug worldPositionPlug = worldPositionArray.elementByLogicalIndex(0, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot get world position plug");

        MPlug controlPointsArray = curveFn.findPlug("controlPoints", true);
        MPlug controlPointPlug = controlPointsArray.elementByLogicalIndex(cvIndex, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot get control point plug");

        return dgModifier.connect(worldPositionPlug, controlPointPlug);
    }
}


void* PreBuildBoneChain::creator()
{
    return new PreBuildBoneChain();
}

MSyntax PreBuildBoneChain::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}


MStatus PreBuildBoneChain::doIt(const MArgList& args)
{
    MStatus status;
    BoneChainDefinition chain;

    status = ChainCommandUtils::parseDefinition(syntax(), args, chain);
    RETURN_IF_MAYA_FAILED(status, "Unable to read chain definition");

    // Check before creation
    if (chain.bones.size() < 2)
    {
        MGlobal::displayError("A bone chain requires at least two guides");
        return MS::kFailure;
    }

    for (const BoneDefinition& bone : chain.bones)
    {
        const MString guideName = chain.guideName(bone);

        if (FuncUtils::objectExists(guideName))
        {
            MGlobal::displayError("Guide already exists: " + guideName);
            return MS::kFailure;
        }
    }

    if (FuncUtils::objectExists(chain.guideCurveName()))
    {
        MGlobal::displayError("Guide curve already exists: " + chain.guideCurveName());
        return MS::kFailure;
    }

    std::vector<MObject> guideShapes; // guide locator shapes
    guideShapes.reserve(chain.bones.size()); // reserve memory

    MPointArray curveCVs;

    // Create guide locator and get curve cvs position
    for (const BoneDefinition& bone : chain.bones)
    {
        MObject transform, shape;

        status = createGuideLocator(chain.guideName(bone), bone.position, chain.guideColor, transform, shape);
        RETURN_IF_MAYA_FAILED(status, "Cannot create guide locator");

        guideShapes.push_back(shape);
        curveCVs.append(MPoint(bone.position));
    }

    MDoubleArray knots;

    for (unsigned int i = 0; i < chain.bones.size(); i++)
    {
        knots.append(static_cast<double>(i));
    }

    MFnTransform curveTransformFn;
    MObject curveTransform = curveTransformFn.create(MObject::kNullObj, &status); // create curve transform node
    RETURN_IF_MAYA_FAILED(status, "Failed to create curve transform");
    curveTransformFn.setName(chain.guideCurveName(), false);

    MFnNurbsCurve curveFn;
    MObject curveShape = curveFn.create(curveCVs, knots, 1, MFnNurbsCurve::kOpen, false, false, curveTransform, &status);
    RETURN_IF_MAYA_FAILED(status, "Failed to create curve shape");
    curveFn.setName(chain.guideCurveName() + "Shape", false);

    FuncUtils::setDisplayColor(curveShape, chain.guideCurveColor);

    // connect dependency graph
    MDGModifier dgModifier;
    for (unsigned int i = 0; i < guideShapes.size(); i++)
    {
        status = connectLocatorToCurveCV(guideShapes[i], curveShape, i, dgModifier);
    }
    
    status = dgModifier.doIt();
    RETURN_IF_MAYA_FAILED(status, "Failed to apply guide connections");

    MGlobal::displayInfo("Shoulder, elbow and hand guides created successful.");

    return MS::kSuccess;
}
