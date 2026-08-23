//-----------------------------------------------------------------------------------------
// Common flags: module and side
//-----------------------------------------------------------------------------------------

#pragma once

#include "BoneChainDefinition.h"
#include "RigModuleRegistry.h"

#include <maya/MSyntax.h>
#include <maya/MArgDatabase.h>
#include <maya/MGlobal.h>

namespace ChainCommandUtils
{
	// Parameter rules
	inline MSyntax createSyntax()
	{
		MSyntax syntax;

		syntax.addFlag("-m", "-module", MSyntax::kString);
		syntax.addFlag("-s", "-side", MSyntax::kString);

		return syntax;
	}


	// Get parameters
	inline MStatus parseModuleAndSide(
		const MSyntax& syntax,
		const MArgList& args,
		MString& module,
		MString& side
	)
	{
		MStatus status;

		MArgDatabase database(syntax, args, &status);
		if (!status) return status;

		module = "arm";
		side = "L";

		if (database.isFlagSet("-module"))
		{
			database.getFlagArgument("-module", 0, module);
		}
		if (database.isFlagSet("-side"))
		{
			database.getFlagArgument("-side", 0, side);
		}

		module.toLowerCase();
		side.toUpperCase();

		if (side != "L" && side != "R" && side != "M")
		{
			MGlobal::displayError("Side must be L, R or M");
			return MS::kInvalidParameter;
		}

		return MS::kSuccess;
	}


	inline MStatus parseDefinition(
		const MSyntax& syntax,
		const MArgList& args,
		SingleChainDefinition& definition
	)
	{
		MString module;
		MString side;

		MStatus status = parseModuleAndSide(syntax, args, module, side);
		if (!status) return status;

		return RigModuleRegistry::getChain(module, side, definition);
	}

	inline MStatus parseDefinition(
		const MSyntax& syntax,
		const MArgList& args,
		TreeBoneDefinition& definition
	)
	{
		MString module;
		MString side;

		MStatus status = parseModuleAndSide(syntax, args, module, side);
		if (!status) return status;

		return RigModuleRegistry::getTree(module, side, definition);
	}
}
