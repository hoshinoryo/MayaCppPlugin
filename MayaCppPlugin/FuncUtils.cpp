#include "FuncUtils.h"
#include "StatusUtils.h"

#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MVector.h>
#include <maya/MMatrix.h>


namespace
{
    constexpr double MIN_DIRECTION_LENGTH = 0.000001;
}

bool FuncUtils::objectExists(const MString& objectName)
{
    MSelectionList selectionList;

    return selectionList.add(objectName) == MS::kSuccess;
}

MStatus FuncUtils::setDisplayColor(const MObject& shapeObject, short colorIndex)
{
    MStatus status;
    MFnDependencyNode shapeFn(shapeObject);

    MPlug overrideEnabledPlug = shapeFn.findPlug("overrideEnabled", true);
    status = overrideEnabledPlug.setBool(true);
    RETURN_IF_MAYA_FAILED(status, "Cannot enable display override");

    MPlug overrideColorPlug = shapeFn.findPlug("overrideColor", true);
    status = overrideColorPlug.setShort(colorIndex);
    RETURN_IF_MAYA_FAILED(status, "Cannot set display color");

    return MS::kSuccess;
}

MStatus FuncUtils::setLocatorSize(const MObject& locatorShape, double size)
{
    //MStatus status;
    MFnDependencyNode locatorFn(locatorShape);

    const char* scaleAttributes[] =
    {
        "localScaleX",
        "localScaleY",
        "localScaleZ"
    };

    for (const char* attributeName : scaleAttributes)
    {
        MPlug scalePlug = locatorFn.findPlug(attributeName, true);
        scalePlug.setDouble(size);
    }

    return MS::kSuccess;
}

MStatus FuncUtils::getTransformWorldPosition(const MString& transformNode, MVector& worldPosition)
{
    MStatus status;
    MSelectionList selectionList;

    status = selectionList.add(transformNode);
    RETURN_IF_MAYA_FAILED(status, "Cannot find transform");

    MDagPath transformPath;
    status = selectionList.getDagPath(0, transformPath);
    RETURN_IF_MAYA_FAILED(status, "Cannot get transform DAG path");

    MFnTransform transformFn(transformPath);

    worldPosition = transformFn.getTranslation(MSpace::kWorld);
    RETURN_IF_MAYA_FAILED(status, "Cannot read transform world position");

    return MS::kSuccess;
}

// Use Gram-Schmidt orthogonalization to construct a stable orthogonal coordinate system
// by making the Y axis perpendicular to X while keeping it close to the world up direction.
MStatus FuncUtils::buildAimOrientationMatrix(
    const MVector& startPosition,
    const MVector& endPosition,
    MMatrix& orientationMatrix
)
{
    MVector xAxis = endPosition - startPosition;
    if (xAxis.length() < MIN_DIRECTION_LENGTH)
    {
        MGlobal::displayError("start and end positions are too close");
        return MS::kFailure;
    }
    xAxis.normalize();

    const MVector worldUp(0.0, 1.0, 0.0);
    MVector yAxis = worldUp - xAxis * (worldUp * xAxis);
    if (yAxis.length() < MIN_DIRECTION_LENGTH)
    {
        const MVector fallbackUp(0.0, 0.0, 1.0);
        yAxis = fallbackUp - xAxis * (fallbackUp * xAxis);
    }
    yAxis.normalize();

    MVector zAxis = xAxis ^ yAxis;
    zAxis.normalize();

    yAxis = zAxis ^ xAxis;
    yAxis.normalize();

    orientationMatrix.setToIdentity();

    orientationMatrix[0][0] = xAxis.x;
    orientationMatrix[0][1] = xAxis.y;
    orientationMatrix[0][2] = xAxis.z;

    orientationMatrix[1][0] = yAxis.x;
    orientationMatrix[1][1] = yAxis.y;
    orientationMatrix[1][2] = yAxis.z;

    orientationMatrix[2][0] = zAxis.x;
    orientationMatrix[2][1] = zAxis.y;
    orientationMatrix[2][2] = zAxis.z;

    return MS::kSuccess;
}
