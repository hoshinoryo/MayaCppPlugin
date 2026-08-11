#pragma once

#include <maya/MString.h>
#include <maya/MStatus.h>

namespace FuncUtils
{
    bool objectExists(const MString& objectName);
    MStatus setDisplayColor(const MObject& shapeObject, short colorIndex);
    MStatus setLocatorSize(const MObject& locatorShape, double size);

    MStatus getTransformWorldPosition(const MString& transformNode, MVector& worldPosition);
    MStatus buildAimOrientationMatrix(const MVector& startPosition, const MVector& endPosition, MMatrix& orientationMatrix);
}