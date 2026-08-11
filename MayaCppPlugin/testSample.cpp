#include "testSample.h"

#include <maya/MStatus.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MIOStream.h>
#include <maya/MFnMesh.h>
#include <maya/MPointArray.h>

void* testSample::creator()
{
    return new testSample;
}

MStatus testSample::doIt(const MArgList& args)
{
    MStatus status;

    MGlobal::displayInfo("-------------testSample-------------");

    MSelectionList slist;
    MGlobal::getActiveSelectionList(slist);

    MDagPath meshDagPath;
    slist.getDagPath(0, meshDagPath);
    meshDagPath.extendToShape();

    MFnMesh meshFn(meshDagPath);

    int numVertices = meshFn.numVertices();
    MGlobal::displayInfo(MString("numVertices: ") + numVertices);

    int numPolygons = meshFn.numPolygons();
    MGlobal::displayInfo(MString("numPolygons: ") + numPolygons);

    MPointArray vertexPosArray;
    meshFn.getPoints(vertexPosArray, MSpace::kObject);
    for (unsigned int i = 0; i < vertexPosArray.length(); ++i) {
        MPoint p = vertexPosArray[i];
        MString msg;
        msg.format("vertexPosArrayLocal[^1s]: (^2s, ^3s, ^4s)",
            MString() + i,
            MString() + p.x,
            MString() + p.y,
            MString() + p.z
        );

        MGlobal::displayInfo(msg);
    }

    meshFn.getPoints(vertexPosArray, MSpace::kWorld);
    for (unsigned int i = 0; i < vertexPosArray.length(); ++i) {
        MPoint p = vertexPosArray[i];
        MString msg;
        msg.format("vertexPosArrayWorld[^1s]: (^2s, ^3s, ^4s)",
            MString() + i,
            MString() + p.x,
            MString() + p.y,
            MString() + p.z
        );

        MGlobal::displayInfo(msg);
    }

    for (int j = 0; j < numPolygons; j++)
    {
        MIntArray vertexList;
        meshFn.getPolygonVertices(j, vertexList);
        for (unsigned int k = 0; k < vertexList.length(); k++)
        {
            MString msg;
            msg.format("faceId: ^1s vertexId: ^2s vertexPosArray kWorld: (^3s, ^4s, ^5s)",
                MString() + j,
                MString() + vertexList[k],
                MString() + vertexPosArray[vertexList[k]].x,
                MString() + vertexPosArray[vertexList[k]].y,
                MString() + vertexPosArray[vertexList[k]].z
            );

            MGlobal::displayInfo(msg);
        }
    }


    return status;
}
