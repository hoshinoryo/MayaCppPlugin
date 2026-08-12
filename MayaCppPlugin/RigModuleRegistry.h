#pragma once

#include "BoneChainDefinition.h"

namespace RigModuleRegistry
{
	MStatus getChain(const MString& module, const MString& side, BoneChainDefinition& result);
}