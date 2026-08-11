///-------------------------------------------------------------------------------
///shoulder_guide Å® shoulder_jnt
///elbow_guide    Å® elbow_jnt
///hand_guide     Å® hand_jnt
///-------------------------------------------------------------------------------

#include "PreBuildBoneChain.h"
#include "StatusUtils.h"
#include "FuncUtils.h"

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


MStatus PreBuildBoneChain::doIt(const MArgList& args)
{
    MStatus status;

    if (FuncUtils::objectExists("shoulder_guide") ||
        FuncUtils::objectExists("elbow_guide") ||
        FuncUtils::objectExists("hand_guide") ||
        FuncUtils::objectExists("arm_guide_curve")
        )
    {
        MGlobal::displayError("Arm guides already exists."
            "Delete the existing guides before creating them again."
        );

        return MS::kFailure;
    }

    // Create guide transforms and shapes
    MObject shoulderTransform, shoulderShape;
    MObject elbowTransform, elbowShape;
    MObject handTransform, handShape;

    createGuideLocator("shoulder_guide", MVector(0.0, 10.0, 0.0), 17, shoulderTransform, shoulderShape);
    createGuideLocator("elbow_guide",    MVector(5.0, 10.0, 0.0), 17, elbowTransform, elbowShape);
    createGuideLocator("hand_guide",     MVector(9.0, 10.0, 0.0), 17, handTransform, handShape);

    // create cv curve
    // cv[0] = shoulder, cv[1] = elbow, cv[2] = hand, degree = 1
    MPointArray curveCVs;
    curveCVs.append(MPoint(0.0, 10.0, 0.0));
    curveCVs.append(MPoint(5.0, 10.0, 0.0));
    curveCVs.append(MPoint(9.0, 10.0, 0.0));

    MDoubleArray knots;
    knots.append(0.0);
    knots.append(1.0);
    knots.append(2.0);

    MFnTransform curveTransformFn;
    MObject curveTransform = curveTransformFn.create(MObject::kNullObj, &status); // create curve transform node
    RETURN_IF_MAYA_FAILED(status, "Failed to create curve transform");

    curveTransformFn.setName("arm_guide_curve", false);

    MFnNurbsCurve curveFn;
    MObject curveShape = curveFn.create(curveCVs, knots, 1, MFnNurbsCurve::kOpen, false, false, curveTransform, &status);
    RETURN_IF_MAYA_FAILED(status, "Failed to create curve shape");

    curveFn.setName("arm_guide_curveShape", false);

    status = FuncUtils::setDisplayColor(curveShape, 18);
    RETURN_IF_MAYA_FAILED(status, "Cannot set curve color");

    // connect dependency graph
    MDGModifier dgModifier;
    status = connectLocatorToCurveCV(shoulderShape, curveShape, 0, dgModifier);
    RETURN_IF_MAYA_FAILED(status, "shoulder connection failed");

    status = connectLocatorToCurveCV(elbowShape, curveShape, 1, dgModifier);
    RETURN_IF_MAYA_FAILED(status, "elbow connection failed");

    status = connectLocatorToCurveCV(handShape, curveShape, 2, dgModifier);
    RETURN_IF_MAYA_FAILED(status, "hand connection failed");

    status = dgModifier.doIt();
    RETURN_IF_MAYA_FAILED(status, "Failed to connect guides to curve");

    MGlobal::displayInfo("Shoulder, elbow and hand guides created successful.");

    return MS::kSuccess;
}
