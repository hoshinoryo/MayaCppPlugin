#ifndef CUSTOMSPHERE_H
#define CUSTOMSPHERE_H

#include <maya/MGlobal.h>
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>

/// <summary>
/// Creates a lots of sphere object.
/// </summary>
class CustomSphere : public MPxCommand
{
public:

	virtual ~CustomSphere() = default;

	// @brief our creator function called when the class is created
	// the returns a new instance of this class
	static void* creator();

	MStatus doIt(const MArgList& args) override;
	MStatus redoIt() override;
	MStatus undoIt() override;

	bool isUndoable() const override;

private:

	int m_count;
};

#endif
