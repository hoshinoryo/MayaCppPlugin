#pragma once

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#define RETURN_IF_MAYA_FAILED(status, message)                                          \
	do                                                                                  \
	{                                                                                   \
		if (!(status))                                                                  \
		{                                                                               \
			MGlobal::displayError(MString(message) + ": " + (status).errorString());    \
			return status;                                                              \
		}                                                                               \
	}while(false)