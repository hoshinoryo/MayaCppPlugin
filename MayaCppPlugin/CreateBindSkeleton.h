#pragma once

#include <maya/MStatus.h>
#include <maya/MPxCommand.h>

class CreateBindSkeleton : public MPxCommand
{
public:

	static void* creator();

	MStatus doIt(const MArgList& args) override;
};