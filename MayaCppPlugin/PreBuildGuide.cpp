#include "PreBuildGuide.h"
#include "StatusUtils.h"
#include "FuncUtils.h"
#include "ChainCommandUtils.h"
#include "BoneChainDefinition.h"

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MFnTransform.h>
#include <maya/MDGModifier.h>
#include <maya/MGlobal.h>
#include <maya/MVector.h>
#include <maya/MPointArray.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MDagModifier.h>
#include <maya/MDagPath.h>


namespace
{
    constexpr double GUIDE_LOCATOR_SIZE = 0.5;
    constexpr double LABEL_HEIGHT = 0.8;

    MStatus setNodeUnselectable(const MObject& node)
    {
        MStatus status;
        MFnDependencyNode nodeFn(node);

        MPlug overrideEnabled = nodeFn.findPlug("overrideEnabled", true);
        overrideEnabled.setBool(true);

        MPlug overrideDisplayTypePlug = nodeFn.findPlug("overrideDisplayType", true);
        overrideDisplayTypePlug.setShort(2); // 0 - Normal, 1 - Template, 2 - Reference

        return MS::kSuccess;
    }

    MStatus createGuideLabel(const MString& guideName, const MString& label, const MObject& guideTransform)
    {
        MStatus status;
        MFnTransform labelTransformFn; // child of guide locator

        MObject labelTransform = labelTransformFn.create(guideTransform, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create guide label transform");

        labelTransformFn.setName(guideName + "_label", false);
        labelTransformFn.setTranslation(MVector(0.0, LABEL_HEIGHT, 0.0), MSpace::kTransform);

        MFnDagNode annotationDagFn; // set annotation's attribute
        MObject annotationShape = annotationDagFn.create("annotationShape", labelTransform, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot create annotationShape");
        annotationDagFn.setName(guideName + "_labelShape", false);

        MFnDependencyNode annotationNodeFn(annotationShape);
        MPlug textPlug = annotationNodeFn.findPlug("text", true);
        textPlug.setString(label);
        MPlug displayArrowPlug = annotationNodeFn.findPlug("displayArrow", true);
        displayArrowPlug.setBool(false);

        setNodeUnselectable(labelTransform);
        setNodeUnselectable(annotationShape);

        return MS::kSuccess;
    }

    MStatus createGuideLocator(
        const MString& transformName,
        const MString& label,
        const MVector& localPosition,
        short colorIndex,
        const MObject& parentTransform,
        MObject& locatorTransform,
        MObject& locatorShape
    )
    {
        MStatus status;
        MFnTransform transformFn; // transform node for locator

        locatorTransform = transformFn.create(parentTransform, &status);
        RETURN_IF_MAYA_FAILED(status, "Failed to create locator transform node");

        transformFn.setName(transformName, false);
        transformFn.setTranslation(localPosition, MSpace::kTransform); // local translation with parent

        MFnDagNode locatorShapeFn;
        locatorShape = locatorShapeFn.create("locator", locatorTransform);
        locatorShapeFn.setName(transformName + "Shape", false);

        status = FuncUtils::setLocatorSize(locatorShape, 0.65);
        RETURN_IF_MAYA_FAILED(status, "Cannot set locator size");

        // Create label
        FuncUtils::setDisplayColor(locatorShape, colorIndex);
        createGuideLabel(transformName, label, locatorTransform);

        return MS::kSuccess;
    }

    MStatus connectLocatorToCurveCV(
        const MObject& locatorShape,
        const MObject& curveShape,
        unsigned int cvIndex,
        MDGModifier& dgModifier
    )
    {
        MStatus status;
        MFnDependencyNode locatorFn(locatorShape);
        MFnDependencyNode curveFn(curveShape);

        MPlug worldPositionArray = locatorFn.findPlug("worldPosition", true);
        MPlug worldPositionPlug = worldPositionArray.elementByLogicalIndex(0, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot get world position plug");

        MPlug controlPointsArray = curveFn.findPlug("controlPoints", true);
        MPlug controlPointPlug = controlPointsArray.elementByLogicalIndex(cvIndex, &status);
        RETURN_IF_MAYA_FAILED(status, "Cannot get control point plug");

        return dgModifier.connect(worldPositionPlug, controlPointPlug);
    }


    MStatus createSingleChainGuides(const SingleChainDefinition& chain,
        const MObject& initialParentTransform = MObject::kNullObj,
        const MVector& initialParentPosition = MVector::zero
    )
    {
        MStatus status;

        std::vector<MObject> guideShapes; // guide locator shapes
        guideShapes.reserve(chain.bones.size()); // reserve memory

        MPointArray curveCVs;

        MObject parentGuideTransform = initialParentTransform;
        MVector preWorldPosition = initialParentPosition;

        // Create guide locator and get curve cvs position
        for (size_t i = 0; i < chain.bones.size(); i++)
        {
            const BoneBase& bone = chain.bones[i];
            MVector localPositon;

            if (i == 0 && chain.module != "hand")
            {
                localPositon = bone.position;
            }
            else
            {
                localPositon = bone.position - preWorldPosition;
            }

            MObject guideTransform, guideShape;

            status = createGuideLocator(
                chain.guideName(bone),
                bone.label,
                localPositon,
                chain.guideColor,
                parentGuideTransform,
                guideTransform,
                guideShape
            );
            RETURN_IF_MAYA_FAILED(status, "Cannot create guide locator");

            guideShapes.push_back(guideShape);
            curveCVs.append(MPoint(bone.position));

            parentGuideTransform = guideTransform;
            preWorldPosition = bone.position;
        }

        MDoubleArray knots;

        for (unsigned int i = 0; i < chain.bones.size(); i++)
        {
            knots.append(static_cast<double>(i));
        }

        MFnTransform curveTransformFn;
        MObject curveTransform = curveTransformFn.create(MObject::kNullObj, &status); // create curve transform node
        RETURN_IF_MAYA_FAILED(status, "Failed to create curve transform");
        curveTransformFn.setName(chain.guideCurveName(), false);

        MFnNurbsCurve curveFn;
        MObject curveShape = curveFn.create(curveCVs, knots, 1, MFnNurbsCurve::kOpen, false, false, curveTransform, &status);
        RETURN_IF_MAYA_FAILED(status, "Failed to create curve shape");
        curveFn.setName(chain.guideCurveName() + "Shape", false);
        setNodeUnselectable(curveTransform);
        setNodeUnselectable(curveShape);

        FuncUtils::setDisplayColor(curveShape, chain.guideCurveColor);

        // connect dependency graph
        MDGModifier dgModifier;
        for (unsigned int i = 0; i < guideShapes.size(); i++)
        {
            status = connectLocatorToCurveCV(guideShapes[i], curveShape, i, dgModifier);
        }

        status = dgModifier.doIt();
        RETURN_IF_MAYA_FAILED(status, "Failed to apply guide connections");

        return MS::kSuccess;
    }

    MStatus createTreeGuides(const TreeBoneDefinition& tree)
    {
        MStatus status;

        const MString parentPrefix = tree.side.length() == 0 || tree.side == "M"
            ? "M_" + tree.parentModule
            : tree.side + "_" + tree.parentModule;
        const MString parentGuideName = parentPrefix + "_" + tree.parentLabel + "_guide";

        MObject parentGuideTransform = MObject::kNullObj;
        MVector parentWorldPosition = MVector::zero;

        if (FuncUtils::objectExists(parentGuideName))
        {
            MDagPath parentGuidePath;
            FuncUtils::getDagPath(parentGuideName, parentGuidePath);
            FuncUtils::getWorldPosition(parentGuideName, parentWorldPosition);
            parentGuideTransform = parentGuidePath.node();
        }

        MObject rootGuideTransform, rootGuideShape;
        const MVector rootLocalPosition = tree.root.position - parentWorldPosition;
        status = createGuideLocator(
            tree.rootGuideName(),
            tree.root.label,
            rootLocalPosition,
            tree.guideColor,
            parentGuideTransform,
            rootGuideTransform,
            rootGuideShape
        );
        RETURN_IF_MAYA_FAILED(status, "Cannot create tree root guide");

        for (const SingleChainDefinition& child : tree.children)
        {
            status = createSingleChainGuides(child, rootGuideTransform, tree.root.position);
            RETURN_IF_MAYA_FAILED(status, "Cannot craete child guide chain");
        }

        MGlobal::displayInfo(tree.prefix() + " tree guide created successfully");

        return MS::kSuccess;
    }
}


void* PreBuildGuide::creator()
{
    return new PreBuildGuide();
}

MSyntax PreBuildGuide::newSyntax()
{
    return ChainCommandUtils::createSyntax();
}


MStatus PreBuildGuide::doIt(const MArgList& args)
{
    MStatus status;
    MString module, side;

    status = ChainCommandUtils::parseModuleAndSide(syntax(), args, module, side);
    RETURN_IF_MAYA_FAILED(status, "Unable to read module and side");

    m_isTreeGuide = module == "hand";

    // Check before creation
    if (m_isTreeGuide)
    {
        status = ChainCommandUtils::parseDefinition(syntax(), args, m_Tree);
        RETURN_IF_MAYA_FAILED(status, "Unable to read tree definition");

        if (FuncUtils::objectExists(m_Tree.rootGuideName()) 
            || FuncUtils::objectExists(m_Tree.rootGuideName() + "_label"))
        {
            MGlobal::displayError("Guide already exists: " + m_Tree.rootGuideName());
            return MS::kFailure;
        }

        for (const SingleChainDefinition& child : m_Tree.children)
        {
            if (child.bones.size() < 2)
            {
                MGlobal::displayError("A bone chain requires at least two guides: " + child.prefix());
                return MS::kFailure;
            }

            for (const BoneBase& bone : child.bones)
            {
                const MString guideName = child.guideName(bone);

                if (FuncUtils::objectExists(guideName))
                {
                    MGlobal::displayError("Guide already exists: " + guideName);
                    return MS::kFailure;
                }

                if (FuncUtils::objectExists(guideName + "_label"))
                {
                    MGlobal::displayError("Guide label already exists: " + guideName + "_label");
                    return MS::kFailure;
                }
            }

            if (FuncUtils::objectExists(child.guideCurveName()))
            {
                MGlobal::displayError("Guide curve already exists: " + m_Chain.guideCurveName());
                return MS::kFailure;
            }
        }
    }
    else
    {
        status = ChainCommandUtils::parseDefinition(syntax(), args, m_Chain);
        RETURN_IF_MAYA_FAILED(status, "Unable to read chain definition");

        if (m_Chain.bones.size() < 2)
        {
            MGlobal::displayError("A bone chain requires at least two guides: " + m_Chain.prefix());
            return MS::kFailure;
        }

        for (const BoneBase& bone : m_Chain.bones)
        {
            const MString guideName = m_Chain.guideName(bone);

            if (FuncUtils::objectExists(guideName))
            {
                MGlobal::displayError("Guide already exists: " + guideName);
                return MS::kFailure;
            }

            if (FuncUtils::objectExists(guideName + "_label"))
            {
                MGlobal::displayError("Guide label already exists: " + guideName + "_label");
                return MS::kFailure;
            }
        }

        if (FuncUtils::objectExists(m_Chain.guideCurveName()))
        {
            MGlobal::displayError("Guide curve already exists: " + m_Chain.guideCurveName());
            return MS::kFailure;
        }
    }

    return redoIt();
}

MStatus PreBuildGuide::redoIt()
{
    MStatus status;

    if (m_isTreeGuide)
    {
        status = createTreeGuides(m_Tree);
        RETURN_IF_MAYA_FAILED(status, "Unable to create tree guides");

        MGlobal::displayInfo(m_Tree.prefix() + " guides created successful.");
    }
    else
    {
        status = createSingleChainGuides(m_Chain);
        RETURN_IF_MAYA_FAILED(status, "Unable to create chain guides");

        MGlobal::displayInfo(m_Chain.prefix() + " guides created successful.");
    }

    return MS::kSuccess;
}

MStatus PreBuildGuide::undoIt()
{
    return MStatus();
}

bool PreBuildGuide::isUndoable() const
{
    return true;
}
