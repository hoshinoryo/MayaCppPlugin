#include "FuncUtils.h"
#include "StatusUtils.h"

#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MVector.h>
#include <maya/MMatrix.h>
#include <maya/MQuaternion.h>


namespace
{
    constexpr double MIN_DIRECTION_LENGTH = 0.000001;
}


bool FuncUtils::objectExists(const MString& objectName)
{
    MSelectionList selectionList;

    return selectionList.add(objectName) == MS::kSuccess;
}

MStatus FuncUtils::executeMayaCommand(const MString& command, const MString& errorMessage)
{
    MStatus status = MGlobal::executeCommand(command, false, true);
    RETURN_IF_MAYA_FAILED(status, errorMessage);

    return MS::kSuccess;
}

MStatus FuncUtils::getDagPath(const MString& nodeName, MDagPath& dagPath)
{
    MStatus status;
    MSelectionList selectionList;

    status = selectionList.add(nodeName);
    RETURN_IF_MAYA_FAILED(status, "Cannot find DAG node");

    status = selectionList.getDagPath(0, dagPath);
    RETURN_IF_MAYA_FAILED(status, "Cannot get DAG path");

    return MS::kSuccess;
}

MStatus FuncUtils::getShapeFromTransform(const MString& transformName, MDagPath& shapePath)
{
    MStatus status;

    getDagPath(transformName, shapePath);

    if (!shapePath.hasFn(MFn::kTransform))
    {
        MGlobal::displayError("Node is not a transform: " + transformName);
        return MS::kFailure;
    }

    status = shapePath.extendToShape();
    RETURN_IF_MAYA_FAILED(status, "Cannot extend path to shape");

    return MS::kSuccess;
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

MStatus FuncUtils::getWorldPosition(const MString& transformNode, MVector& worldPosition)
{
    MStatus status;
    MDagPath transformPath;

    getDagPath(transformNode, transformPath);

    MFnTransform transformFn(transformPath);

    worldPosition = transformFn.getTranslation(MSpace::kWorld, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot read transform world position");

    return MS::kSuccess;
}

MStatus FuncUtils::setWorldPosition(const MString& transformNode, const MVector& worldPosition)
{
    MStatus status;
    MDagPath transformPath;

    getDagPath(transformNode, transformPath);

    MFnTransform transformFn(transformPath);

    status = transformFn.setTranslation(worldPosition, MSpace::kWorld);
    RETURN_IF_MAYA_FAILED(status, "Cannot set transform world position");

    return MS::kSuccess;
}

MStatus FuncUtils::matchWorldPositionAndRotation(const MString& destNode, const MString& sourceNode)
{
    MStatus status;
    MDagPath destPath, sourcePath;

    getDagPath(destNode, destPath);
    getDagPath(sourceNode, sourcePath);

    const MMatrix sourceWorldMatrix = sourcePath.inclusiveMatrix(&status);

    MTransformationMatrix sourceTransform(sourceWorldMatrix);

    const MVector sourceTranslation = sourceTransform.getTranslation(MSpace::kTransform, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot read source translation");

    const MQuaternion sourceRotation = sourceTransform.rotation();

    MTransformationMatrix desiredWorldTransform;
    desiredWorldTransform.setTranslation(sourceTranslation, MSpace::kTransform);
    desiredWorldTransform.rotateTo(sourceRotation);

    MMatrix parentWorldMatrix;
    parentWorldMatrix.setToIdentity();

    if (destPath.length() > 1)
    {
        MDagPath parentPath = destPath;
        parentPath.pop();

        parentWorldMatrix = parentPath.inclusiveMatrix(&status);
        RETURN_IF_MAYA_FAILED(status, "Cannot read destination parent matrix");
    }

    const MMatrix destLocalMatrix = desiredWorldTransform.asMatrix() * parentWorldMatrix.inverse();
    MTransformationMatrix destTransform(destLocalMatrix);

    MFnTransform destinationFn(destPath);
    destinationFn.set(destTransform);

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

MQuaternion FuncUtils::matrixToQuaternion(const MMatrix& matrix)
{
    return MTransformationMatrix (matrix).rotation();
}
