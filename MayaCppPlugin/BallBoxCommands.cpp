#include "BallBoxCommands.h"
#include <maya/MStatus.h>

void* CreateBallCommand::creator()
{
    return new CreateBallCommand;
}

MStatus CreateBallCommand::doIt(const MArgList& args)
{
    const MStatus status = MGlobal::executeCommand("polySphere");
    if (!status)
    {
        MGlobal::displayInfo("Failed to create ball: " + status.errorString());
    }
    return status;
}

void* CreateBoxCommand::creator()
{
    return new CreateBoxCommand;
}

MStatus CreateBoxCommand::doIt(const MArgList& args)
{
    const MStatus status = MGlobal::executeCommand("polyCube");
    if (!status)
    {
        MGlobal::displayInfo("Failed to create box: " + status.errorString());
    }
    return status;
}
