#pragma once

#include <maya/MGlobal.h>
#include <maya/MPxCommand.h>

/// <summary>
/// Creates a poly ball object.
/// </summary>
class CreateBallCommand : public MPxCommand
{
public:

	// @brief our creator function called when the class is created
	// the returns a new instance of this class
	static void* creator();

	MStatus doIt(const MArgList& args) override;
};

/// <summary>
/// Creates a poly cube object.
/// </summary>
class CreateBoxCommand : public MPxCommand
{
public:
	static void* creator();

	MStatus doIt(const MArgList& args) override;
};