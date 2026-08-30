#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>

#include "CreateJointChain.h"
#include "PreBuildGuide.h"
#include "CreateFkController.h"
#include "CreateIkController.h"
#include "MirrorJointChain.h"
#include "CreateBindSkeleton.h"
#include "OrganizeControllerHierarchy.h"
#include "CreateIkFkSwitch.h"


MStatus initializePlugin(MObject obj)
{
	const char* pluginVendor = "Gu Anyi";
	const char* pluginVersion = "1.0";

	MStatus status;
	MFnPlugin fnPlugin(obj, pluginVendor, pluginVersion);


	status = fnPlugin.registerCommand("CreateJointChain", CreateJointChain::creator, CreateJointChain::newSyntax);
	status = fnPlugin.registerCommand("PreBuildGuide", PreBuildGuide::creator, PreBuildGuide::newSyntax);
	status = fnPlugin.registerCommand("CreateFkController", CreateFkController::creator, CreateFkController::newSyntax);
	status = fnPlugin.registerCommand("CreateIkController", CreateIkController::creator, CreateIkController::newSyntax);
	status = fnPlugin.registerCommand("MirrorJointChain", MirrorJointChain::creator, MirrorJointChain::newSyntax);
	status = fnPlugin.registerCommand("CreateBindSkeleton", CreateBindSkeleton::creator, CreateBindSkeleton::newSyntax);
	status = fnPlugin.registerCommand("OrganizeControllerHierarchy", OrganizeControllerHierarchy::creator);
	status = fnPlugin.registerCommand("CreateIkFkSwitch", CreateIkFkSwitch::creator, CreateIkFkSwitch::newSyntax);

	if (!status)
	{
		status.perror("Plugin commands registeration has failed!");
	}
	else
	{
		MGlobal::displayInfo("Plugin has been initialized!");
	}
	
	return status;
}

MStatus uninitializePlugin(MObject obj)
{
	MStatus status;
	MFnPlugin fnPlugin(obj);


	status = fnPlugin.deregisterCommand("CreateJointChain");
	status = fnPlugin.deregisterCommand("PreBuildGuide");
	status = fnPlugin.deregisterCommand("CreateFkController");
	status = fnPlugin.deregisterCommand("CreateIkController");
	status = fnPlugin.deregisterCommand("MirrorJointChain");
	status = fnPlugin.deregisterCommand("CreateBindSkeleton");
	status = fnPlugin.deregisterCommand("OrganizeControllerHierarchy");
	status = fnPlugin.deregisterCommand("CreateIkFkSwitch");

	if (!status)
	{
		status.perror("Plugin commands de-registeration has failed!");
	}
	else
	{
		MGlobal::displayInfo("Plugin has been uninitialized!");
	}

	return status;
}