#pragma once

#include <maya/MStatus.h>
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>

class PreBuildBoneChain : public MPxCommand
{
public:

	static void* creator();

	MStatus doIt(const MArgList& args) override;
};