#pragma once

#include <maya/MStatus.h>
#include <maya/MPxCommand.h>

#include "BoneChainDefinition.h"

class CreateIkController : public MPxCommand
{
public:

	static void* creator();
	static MSyntax newSyntax();

	MStatus doIt(const MArgList& args) override;
	MStatus redoIt() override;
	MStatus undoIt() override;

	bool isUndoable() const override;

private:

	SingleChainDefinition m_Chain;
};