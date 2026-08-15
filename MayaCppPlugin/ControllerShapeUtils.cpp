//-----------------------------------------------------------------------------------------
// 
// Controller shape creation utilities
// 
//-----------------------------------------------------------------------------------------

#include "ControllerShapeUtils.h"
#include "StatusUtils.h"
#include "FuncUtils.h"

#include <maya/MPointArray.h>
#include <maya/MPoint.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MFnNurbsCurve.h>

// Repeat the first point at the end
namespace
{
    MPointArray boxPoints()
    {
        MPointArray points;

        points.append(MPoint(-1.0, -1.0, -1.0));
        points.append(MPoint(-1.0, -1.0,  1.0));
        points.append(MPoint( 1.0, -1.0,  1.0));
        points.append(MPoint( 1.0, -1.0, -1.0));
        points.append(MPoint(-1.0, -1.0, -1.0));
        points.append(MPoint(-1.0,  1.0, -1.0));
        points.append(MPoint(-1.0,  1.0,  1.0));
        points.append(MPoint(-1.0, -1.0,  1.0));
        points.append(MPoint(-1.0,  1.0,  1.0));
        points.append(MPoint( 1.0,  1.0,  1.0));
        points.append(MPoint( 1.0, -1.0,  1.0));
        points.append(MPoint( 1.0,  1.0,  1.0));
        points.append(MPoint( 1.0,  1.0, -1.0));
        points.append(MPoint( 1.0, -1.0, -1.0));
        points.append(MPoint( 1.0,  1.0, -1.0));
        points.append(MPoint(-1.0,  1.0, -1.0));

        return points;
    }

    MPointArray pyramidPoints()
    {
        MPointArray points;

        points.append(MPoint(0.0, -0.5, -0.5));
        points.append(MPoint(0.0, -0.5,  0.5));
        points.append(MPoint(0.0,  0.5,  0.5));
        points.append(MPoint(0.0,  0.5, -0.5));
        points.append(MPoint(0.0, -0.5, -0.5));
        points.append(MPoint(1.0,  0.0,  0.0));
        points.append(MPoint(0.0, -0.5,  0.5));
        points.append(MPoint(0.0,  0.5,  0.5));
        points.append(MPoint(1.0,  0.0,  0.0));
        points.append(MPoint(0.0,  0.5, -0.5));

        return points;
    }

    MPointArray arrowPoints()
    {
        MPointArray points;

        points.append(MPoint(0.0, 2.5,  0.0));
        points.append(MPoint(0.0, 1.5, -1.0));
        points.append(MPoint(0.0, 1.5, -0.5));
        points.append(MPoint(0.0, 0.5, -0.5));
        points.append(MPoint(0.0, 0.5, -1.5));
        points.append(MPoint(0.0, 1.0, -1.5));
        points.append(MPoint(0.0, 0.0, -2.5));

        points.append(MPoint(0.0, -1.0, -1.5));
        points.append(MPoint(0.0, -0.5, -1.5));
        points.append(MPoint(0.0, -0.5, -0.5));
        points.append(MPoint(0.0, -1.5, -0.5));
        points.append(MPoint(0.0, -1.5, -1.0));
        points.append(MPoint(0.0, -2.5,  0.0));

        points.append(MPoint(0.0, -1.5, 1.0));
        points.append(MPoint(0.0, -1.5, 0.5));
        points.append(MPoint(0.0, -0.5, 0.5));
        points.append(MPoint(0.0, -0.5, 1.5));
        points.append(MPoint(0.0, -1.0, 1.5));
        points.append(MPoint(0.0,  0.0, 2.5));

        points.append(MPoint(0.0, 1.0, 1.5));
        points.append(MPoint(0.0, 0.5, 1.5));
        points.append(MPoint(0.0, 0.5, 0.5));
        points.append(MPoint(0.0, 1.5, 0.5));
        points.append(MPoint(0.0, 1.5, 1.0));
        points.append(MPoint(0.0, 2.5, 0.0));

        return points;
    }

    MPointArray lollipopPoints()
    {
        MPointArray points;

        points.append(MPoint(0.0, 0.0,  0.0));
        points.append(MPoint(0.0, 2.0,  0.0));
        points.append(MPoint(0.0, 2.5, -0.5));
        points.append(MPoint(0.0, 3.0,  0.0));
        points.append(MPoint(0.0, 2.5,  0.5));
        points.append(MPoint(0.0, 2.0,  0.0));

        return points;
    }

    MPointArray getShapePoints(ControllerShapeUtils::ShapeType shapeType)
    {
        switch (shapeType)
        {
        case ControllerShapeUtils::ShapeType::Box:
            return boxPoints();
            break;

        case ControllerShapeUtils::ShapeType::Pyramid:
            return pyramidPoints();
            break;

        case ControllerShapeUtils::ShapeType::Arrow:
            return arrowPoints();
            break;

        case ControllerShapeUtils::ShapeType::Lollipop:
            return lollipopPoints();
            break;

        default:
            MGlobal::displayError("Shape type does not contain CV points");
            return MPointArray();
            break;
        }
    }

    MStatus createCVCircle(const MString& controllerName, double radius, MObject& controllerTransform)
    {
        MStatus status;

        MString radiusString;
        radiusString.set(radius);

        MString command;

        // Create circle controller
        command.format(
            "circle -name \"^1s\" -normal 1 0 0 -radius ^2s -constructionHistory false",
            controllerName,
            radiusString);
        FuncUtils::executeMayaCommand(command, "Cannot create FK controller circle");

        MDagPath controllerPath;
        status = FuncUtils::getDagPath(controllerName, controllerPath);
        RETURN_IF_MAYA_FAILED(status, "Cannot find controller transform");

        controllerTransform = controllerPath.node();

        return MS::kSuccess;
    }

    MStatus createControllerGroup(const MString& controllerName)
    {
        const MString groupName = ControllerShapeUtils::controllerGroupName(controllerName);

        // Create empty group
        MString command;
        command.format("group -name \"^1s\" \"^2s\"", groupName, controllerName);

        return FuncUtils::executeMayaCommand(command, "Cannot create controller group");
    }
}


MString ControllerShapeUtils::controllerGroupName(const MString& controllerName)
{
    return controllerName + "_grp";
}

MStatus ControllerShapeUtils::createController(
    const MString& controllerName,
    ShapeType shapeType,
    double size,
    MObject& controllerTransform
)
{
    MStatus status;

    if (size <= 0.0)
    {
        MGlobal::displayError("Controller size must be greater than zero");
        return MS::kInvalidParameter;
    }

    if (shapeType == ShapeType::Circle)
    {
        status = createCVCircle(controllerName, size, controllerTransform);
        RETURN_IF_MAYA_FAILED(status, "Unable to create controller");
    }
    else
    {
        const MPointArray points = getShapePoints(shapeType);

        if (points.length() < 2)
        {
            MGlobal::displayError("Controller requires at least two CV points");
            return MS::kInvalidParameter;
        }

        status = createCurve(controllerName, points, size, controllerTransform);
        RETURN_IF_MAYA_FAILED(status, "Unable to create controller");
    }

    createControllerGroup(controllerName); // create controller group

    return MS::kSuccess;
}

// CV to curve
MStatus ControllerShapeUtils::createCurve(
    const MString& controllerName,
    const MPointArray& points,
    double size,
    MObject& controllerTransform
)
{
    MStatus status;

    if (points.length() < 2)
    {
        MGlobal::displayError("A degree 1 curve requires at least two CV points");
        return MS::kInvalidParameter;
    }

    MPointArray scaledPoints = points;

    for (unsigned int i = 0; i < scaledPoints.length(); i++)
    {
        scaledPoints[i].x *= size;
        scaledPoints[i].y *= size;
        scaledPoints[i].z *= size;
    }

    MFnTransform transformFn;
    controllerTransform = transformFn.create(MObject::kNullObj, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot create controller transform");
    transformFn.setName(controllerName, false);

    MDoubleArray knots; // 1 degree curve uses one knot for each CVs
    for (unsigned int i = 0; i < scaledPoints.length(); i++)
    {
        knots.append(static_cast<double>(i));
    }

    MFnNurbsCurve curveFn;
    curveFn.create(scaledPoints, knots, 1, MFnNurbsCurve::kOpen, false, false, controllerTransform, &status);
    RETURN_IF_MAYA_FAILED(status, "Cannot create controller curve");
    curveFn.setName(controllerName + "Shape", false);

    return MS::kSuccess;
}



//-----------------------------------------------------------------------------------------
// 
// åƒÇ—èoÇµä÷åWÅF
// 
// CreateFkController / CreateIkController
//   Å´
// createController()
//   Å´
// shapeType
//   Å´
// createCVCircle() / createCurve()
// 
// 
//-----------------------------------------------------------------------------------------