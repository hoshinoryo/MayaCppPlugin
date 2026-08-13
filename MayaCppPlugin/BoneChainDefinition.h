///----------------------------------------------------------------------------------------
///
/// [Bone chain definition]
/// Definition of bone and bone chain.
/// 
///----------------------------------------------------------------------------------------

#pragma once

#include <maya/MString.h>
#include <maya/MVector.h>
#include <vector>

struct BoneDefinition
{
	MString label; // shoulder/elbow...
	MVector position;
	double constrollerRadius = 1.0;
};

struct BoneChainDefinition
{
	MString module; // arm/leg/spine...
	MString side;   // L/R/M

	std::vector<BoneDefinition> bones;

	short guideColor = 17;
	short guideCurveColor = 18;
	short controllerColor = 6;

	MString prefix() const
	{
		if (side.length() == 0 || side == "M")
		{
			return "M_" + module;
		}

		return side + "_" + module;
	}

	MString guideName(const BoneDefinition& bone) const
	{
		return prefix() + "_" + bone.label + "_guide";
	}

	MString guideCurveName() const
	{
		return prefix() + "_guide_curve";
	}

	MString jointName(const BoneDefinition& bone, const MString& chainType) const
	{
		return prefix() + "_" + bone.label + "_" + chainType + "_jnt";
	}
};
