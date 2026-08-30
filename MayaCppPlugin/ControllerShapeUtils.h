//-----------------------------------------------------------------------------------------
// 
// Controller shape creation utilities
// 
//-----------------------------------------------------------------------------------------

#pragma once

#include <maya/MStatus.h>

namespace ControllerShapeUtils
{
	enum class ShapeType
	{
		Circle,  // Maya command
		Box,
		Pyramid,
		Arrow,
		Lollipop,
		Sphere
	};

	MString controllerGroupName(const MString& controllerName);

	MStatus createController(const MString& controllerName,
		ShapeType shapeType,
		double size,
		MObject& controllerTransform
	);

	MStatus createCurve(
		const MString& controllerName,
		const MPointArray& points,
		double size,
		MObject& controllerTransform
	);
}