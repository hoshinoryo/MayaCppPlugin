#pragma once

#include <maya/MStatus.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

class CreateBindSkeleton : public MPxCommand
{
public:

	static void* creator();
	static MSyntax newSyntax();

	MStatus doIt(const MArgList& args) override;
};