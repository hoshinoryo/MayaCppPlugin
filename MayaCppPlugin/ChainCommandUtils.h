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
	inline MStatus parseDefinition(const MSyntax& syntax, const MArgList& args, BoneChainDefinition& definition)
	{
		MStatus status;

		MArgDatabase database(syntax, args, &status);
		if (!status) return status;

		MString module = "arm";
		MString side = "L";

		if (database.isFlagSet("-module"))
		{
			database.getFlagArgument("-module", 0, module);
		}
		if (database.isFlagSet("-side"))
		{
			database.getFlagArgument("-side", 0, side);
		}

		if (side != "L" && side != "R" && side != "M")
		{
			MGlobal::displayError("Side must be L, R or M");
			return MS::kInvalidParameter;
		}

		return RigModuleRegistry::getChain(module, side, definition);
	}
}
