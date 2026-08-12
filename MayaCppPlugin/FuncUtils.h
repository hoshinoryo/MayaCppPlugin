#pragma once

#include <maya/MString.h>
#include <maya/MStatus.h>

namespace FuncUtils
{
    bool objectExists(const MString& objectName);
    MStatus executeMayaCommand(const MString& command, const MString& errorMessage);

    MStatus getDagPath(const MString& nodeName, MDagPath& dagPath);
    MStatus getShapeFromTransform(const MString& transformName, MDagPath& shapePath);

    MStatus setDisplayColor(const MObject& shapeObject, short colorIndex);
    MStatus setLocatorSize(const MObject& locatorShape, double size);

    MStatus getTransformWorldPosition(const MString& transformNode, MVector& worldPosition);
    MStatus matchWorldPositionAndRotation(const MString& destinationNode, const MString& sourceNode);
    MStatus buildAimOrientationMatrix(const MVector& startPosition, const MVector& endPosition, MMatrix& orientationMatrix);
    MQuaternion matrixToQuaternion(const MMatrix& matrix);
}