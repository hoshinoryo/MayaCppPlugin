#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>

#include "BallBoxCommands.h"
#include "testSample.h"
//#include "CustomCreateSphere.h"
#include "CreateJointChain.h"
#include "PreBuildBoneChain.h"
#include "CreateFkController.h"

//const MString BallCmd = "ball";
//const MString BoxCmd = "box";

MStatus initializePlugin(MObject obj)
{
	const char* pluginVendor = "Gu Anyi";
	const char* pluginVersion = "0.1";

	MStatus status;
	MFnPlugin fnPlugin(obj, pluginVendor, pluginVersion);

	//MStatus status = fnPlugin.registerCommand(BallCmd, CreateBallCommand::creator);
	//status = fnPlugin.registerCommand(BoxCmd, CreateBoxCommand::creator);
	status = fnPlugin.registerCommand("testSample", testSample::creator);
	//status = fnPlugin.registerCommand("CustomSphere", CustomSphere::creator);
	status = fnPlugin.registerCommand("CreateJointChain", CreateJointChain::creator, CreateJointChain::newSyntax);
	status = fnPlugin.registerCommand("PreBuildBoneChain", PreBuildBoneChain::creator, PreBuildBoneChain::newSyntax);
	status = fnPlugin.registerCommand("CreateFkController", CreateFkController::creator, CreateFkController::newSyntax);
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
	//MStatus status = fnPlugin.deregisterCommand(BallCmd);
	//status = fnPlugin.deregisterCommand(BoxCmd);
	status = fnPlugin.deregisterCommand("testSample");
	//status = fnPlugin.deregisterCommand("CustomSphere");
	status = fnPlugin.deregisterCommand("CreateJointChain");
	status = fnPlugin.deregisterCommand("PreBuildBoneChain");
	status = fnPlugin.deregisterCommand("CreateFkController");
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