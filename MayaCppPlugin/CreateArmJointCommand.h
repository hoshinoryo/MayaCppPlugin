#pragma once

#include <maya/MStatus.h>
#include <maya/MPxCommand.h>

class CreateArmJointCommand : public MPxCommand
{
public:

	static void* creator();

	MStatus doIt(const MArgList& args) override;
};
