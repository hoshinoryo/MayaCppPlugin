from PySide2 import QtCore
from PySide2 import QtWidgets

import maya.cmds as cmds
from maya.app.general.mayaMixin import MayaQWidgetDockableMixin


PLUGIN_NAME = "MayaCppPlugin"

PRE_BUILD_COMMAND_NAME = "PreBuildBoneChain"
CREATE_JOINT_COMMAND_NAME = "CreateArmJoint"

_panel_instance = None


class ArmJointPanel(
    MayaQWidgetDockableMixin,
    QtWidgets.QWidget
):
    WINDOW_TITLE = "Auto Rigging"
    OBJECT_NAME = "MayaCppPluginArmJointPanel"

    def __init__(self, parent=None):
        super(ArmJointPanel, self).__init__(parent=parent)

        self.setObjectName(self.OBJECT_NAME)
        self.setWindowTitle(self.WINDOW_TITLE)
        self.setMinimumWidth(280)

        self.create_widgets()
        self.create_layout()
        self.create_connections()

    def create_widgets(self):
        self.create_guides_button = QtWidgets.QPushButton(
            "Create Locator Guides"
        )

        self.create_arm_button = QtWidgets.QPushButton(
            "Create Arm Joint Chain"
        )

        self.create_guides_button.setMinimumHeight(40)
        self.create_arm_button.setMinimumHeight(40)

    def create_layout(self):
        main_layout = QtWidgets.QVBoxLayout(self)

        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(8)

        main_layout.addWidget(
            self.create_guides_button
        )

        main_layout.addWidget(
            self.create_arm_button
        )

        main_layout.addStretch()

    def create_connections(self):
        self.create_guides_button.clicked.connect(
            self.create_visual_guides
        )

        self.create_arm_button.clicked.connect(
            self.create_arm_joint_chain
        )

    @QtCore.Slot()
    def create_visual_guides(self):
        try:
            self.ensure_plugin_loaded()

            command = getattr(
                cmds,
                PRE_BUILD_COMMAND_NAME
            )

            command()

        except Exception as error:
            QtWidgets.QMessageBox.critical(
                self,
                "Create Visual Guides Failed",
                str(error)
            )

    @QtCore.Slot()
    def create_arm_joint_chain(self):
        try:
            self.ensure_plugin_loaded()

            command = getattr(
                cmds,
                CREATE_JOINT_COMMAND_NAME
            )

            command()

        except Exception as error:
            QtWidgets.QMessageBox.critical(
                self,
                "Create Arm Joint Chain Failed",
                str(error)
            )

    def ensure_plugin_loaded(self):
        if cmds.pluginInfo(
            PLUGIN_NAME,
            query=True,
            loaded=True
        ):
            return

        try:
            cmds.loadPlugin(PLUGIN_NAME)

        except RuntimeError as error:
            raise RuntimeError(
                "Could not load {}.mll\n\n{}".format(
                    PLUGIN_NAME,
                    error
                )
            )


def show_panel():
    global _panel_instance

    workspace_control = (
        ArmJointPanel.OBJECT_NAME +
        "WorkspaceControl"
    )

    if cmds.workspaceControl(
        workspace_control,
        query=True,
        exists=True
    ):
        cmds.deleteUI(workspace_control)

    if _panel_instance is not None:
        try:
            _panel_instance.close()
            _panel_instance.deleteLater()

        except RuntimeError:
            pass

    _panel_instance = ArmJointPanel()

    _panel_instance.show(
        dockable=True,
        area="right",
        floating=True
    )

    return _panel_instance