#pragma once

#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>

/// <summary>
/// Creates a poly ball object.
/// </summary>
class testSample : public MPxCommand
{
public:
	static void* creator();

	MStatus doIt(const MArgList& args) override;
};