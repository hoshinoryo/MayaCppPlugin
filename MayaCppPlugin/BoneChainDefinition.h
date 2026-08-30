///----------------------------------------------------------------------------------------
///
/// [Bone chain definition]
/// Definition of bone and bone chain.
/// Single bone chain and tree bone chain
/// 
///----------------------------------------------------------------------------------------

#pragma once

#include <maya/MString.h>
#include <maya/MVector.h>
#include <vector>

enum class JointBuildMode
{
	FkOnly,
	FkAndIk
};

struct BoneParentRef
{
	MString module;
	MString label;
	MString side;

	bool isValid() const
	{
		return module.length() > 0 && label.length() > 0;
	}
};

struct BoneBase
{
	MString label;
	MVector position;
	double  controllerRadius = 1.0;

	JointBuildMode jointBuildMode = JointBuildMode::FkOnly;

	BoneParentRef parent;

	// Create ik joint or not
	bool buildsJointType(const MString& chainType) const
	{
		if (chainType == "fk")
		{
			return true;
		}

		if (chainType == "ik")
		{
			return jointBuildMode == JointBuildMode::FkAndIk;
		}
		return false;
	}
};

struct BoneStructureBase
{
	MString module; // arm/leg/spine...
	MString side;   // L/R/M

	short guideColor = 14;
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

	MString guideName(const BoneBase& bone) const
	{
		return prefix() + "_" + bone.label + "_guide";
	}

	MString guideCurveName() const
	{
		return prefix() + "_guide_curve";
	}

	/// <summary>
	/// Find bone name
	/// </summary>
	MString jointName(const BoneBase& bone, const MString& chainType) const
	{
		return prefix() + "_" + bone.label + "_" + chainType + "_jnt";
	}
};

// Arm/leg/spine/head
struct SingleChainDefinition : BoneStructureBase
{
	MString chainLabel;

	std::vector<BoneBase> bones;

	// Parent bone setting
	MString parentGuide;
	MString parentJoint;

	MString guideCurveName() const
	{
		if (chainLabel.length() > 0 && chainLabel != module)
		{
			return prefix() + "_" + chainLabel + "_guide_curve";
		}

		return BoneStructureBase::guideCurveName();
	}
};

// Hand/wings
struct TreeBoneDefinition : BoneStructureBase
{
	BoneBase root;

	std::vector<SingleChainDefinition> children;

	// Parent bone setting
	MString parentModule;
	MString parentLabel;

	MString rootJointName(const MString& chainType) const
	{
		return jointName(root, chainType);
	}

	MString rootGuideName() const
	{
		return guideName(root);
	}
};