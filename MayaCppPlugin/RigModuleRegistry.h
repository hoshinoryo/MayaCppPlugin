#pragma once

#include "BoneChainDefinition.h"

namespace RigModuleRegistry
{
	MStatus getChain(const MString& module, const MString& side, SingleChainDefinition& result);
	MStatus getTree(const MString& module, const MString& side, TreeBoneDefinition& result);
}