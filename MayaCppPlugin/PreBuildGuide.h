#pragma once

#include <maya/MStatus.h>
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>

class PreBuildGuide : public MPxCommand
{
public:

	static void* creator();
	static MSyntax newSyntax();

	MStatus doIt(const MArgList& args) override;
};