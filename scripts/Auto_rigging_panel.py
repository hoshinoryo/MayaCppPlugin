from PySide2 import QtCore
from PySide2 import QtWidgets

import maya.cmds as cmds
from maya.app.general.mayaMixin import MayaQWidgetDockableMixin


PLUGIN_NAME = "MayaCppPlugin"

PRE_BUILD_COMMAND_NAME = "PreBuildBoneChain"
CREATE_JOINT_COMMAND_NAME = "CreateJointChain"
CREATE_FK_COMMAND_NAME = "CreateFkController"

# Module management from here
MODULE_SIDES = {
    "arm": ("L", "R"),
    "leg": ("L", "R"),
    "spine": ("M",),
}


_panel_instance = None


class RigBuildPanel(
    MayaQWidgetDockableMixin,
    QtWidgets.QWidget
):
    WINDOW_TITLE = "Auto Rigging"
    OBJECT_NAME = "MayaCppPluginRigBuildPanel"

    def __init__(self, parent=None):
        super(RigBuildPanel, self).__init__(parent=parent)

        self.setObjectName(self.OBJECT_NAME)
        self.setWindowTitle(self.WINDOW_TITLE)
        self.setMinimumWidth(300)

        self.create_widgets()
        self.create_layout()
        self.create_connections()

        self.update_side_options()

    # ------------------------------------------------------------------
    # UI creation
    # ------------------------------------------------------------------

    def create_widgets(self):
        self.module_label = QtWidgets.QLabel("Module")
        self.module_combo_box = QtWidgets.QComboBox()
        self.module_combo_box.addItems(MODULE_SIDES.keys())

        self.side_label = QtWidgets.QLabel("Side")
        self.side_combo_box = QtWidgets.QComboBox()

        self.create_guides_button = QtWidgets.QPushButton(
            "Create Locator Guides"
        )

        self.create_joint_chain_button = QtWidgets.QPushButton(
            "Create Joint Chain"
        )

        self.create_fk_button = QtWidgets.QPushButton(
            "Create FK Controllers"
        )

        self.build_all_button = QtWidgets.QPushButton(
            "Build Joint Chain and FK"
        )

        self.create_guides_button.setMinimumHeight(36)
        self.create_joint_chain_button.setMinimumHeight(36)
        self.create_fk_button.setMinimumHeight(36)
        self.build_all_button.setMinimumHeight(42)

    def create_layout(self):
        selection_layout = QtWidgets.QFormLayout()

        selection_layout.setContentsMargins(0, 0, 0, 0)
        selection_layout.setHorizontalSpacing(12)
        selection_layout.setVerticalSpacing(8)

        selection_layout.addRow(
            self.module_label,
            self.module_combo_box
        )

        selection_layout.addRow(
            self.side_label,
            self.side_combo_box
        )

        button_layout = QtWidgets.QVBoxLayout()
        button_layout.setSpacing(8)

        button_layout.addWidget(
            self.create_guides_button
        )

        button_layout.addWidget(
            self.create_joint_chain_button
        )

        button_layout.addWidget(
            self.create_fk_button
        )

        button_layout.addSpacing(8)
        button_layout.addWidget(
            self.build_all_button
        )

        main_layout = QtWidgets.QVBoxLayout(self)

        main_layout.setContentsMargins(12, 12, 12, 12)
        main_layout.setSpacing(12)

        main_layout.addLayout(selection_layout)
        main_layout.addWidget(self.create_separator())
        main_layout.addLayout(button_layout)
        main_layout.addStretch()

    @staticmethod
    def create_separator():
        separator = QtWidgets.QFrame()

        separator.setFrameShape(
            QtWidgets.QFrame.HLine
        )

        separator.setFrameShadow(
            QtWidgets.QFrame.Sunken
        )

        return separator

    def create_connections(self):
        self.module_combo_box.currentTextChanged.connect(
            self.update_side_options
        )

        self.create_guides_button.clicked.connect(
            self.create_locator_guides
        )

        self.create_joint_chain_button.clicked.connect(
            self.create_joint_chain
        )

        self.create_fk_button.clicked.connect(
            self.create_fk_controllers
        )

        self.build_all_button.clicked.connect(
            self.build_joint_chain_and_fk
        )

    # ------------------------------------------------------------------
    # Current module settings
    # ------------------------------------------------------------------

    def current_module(self):
        return self.module_combo_box.currentText()

    def current_side(self):
        return self.side_combo_box.currentText()

    def current_command_arguments(self):
        return {
            "module": self.current_module(),
            "side": self.current_side(),
        }

    @QtCore.Slot()
    def update_side_options(self):
        module_name = self.current_module()
        allowed_sides = MODULE_SIDES.get(
            module_name,
            ("M",)
        )

        previous_side = self.current_side()

        self.side_combo_box.blockSignals(True)
        self.side_combo_box.clear()
        self.side_combo_box.addItems(allowed_sides)

        if previous_side in allowed_sides:
            self.side_combo_box.setCurrentText(
                previous_side
            )

        self.side_combo_box.blockSignals(False)

    # ------------------------------------------------------------------
    # Build commands
    # ------------------------------------------------------------------

    @QtCore.Slot()
    def create_locator_guides(self):
        self.run_build_action(
            self._create_locator_guides,
            "Create Locator Guides Failed"
        )

    @QtCore.Slot()
    def create_joint_chain(self):
        self.run_build_action(
            self._create_joint_chain,
            "Create Joint Chain Failed"
        )

    @QtCore.Slot()
    def create_fk_controllers(self):
        self.run_build_action(
            self._create_fk_controllers,
            "Create FK Controllers Failed"
        )

    @QtCore.Slot()
    def build_joint_chain_and_fk(self):
        self.run_build_action(
            self._build_joint_chain_and_fk,
            "Build Joint Chain and FK Failed"
        )

    def _create_locator_guides(self):
        self.execute_plugin_command(
            PRE_BUILD_COMMAND_NAME
        )

    def _create_joint_chain(self):
        self.execute_plugin_command(
            CREATE_JOINT_COMMAND_NAME
        )

    def _create_fk_controllers(self):
        self.execute_plugin_command(
            CREATE_FK_COMMAND_NAME
        )

    def _build_joint_chain_and_fk(self):
        self.execute_plugin_command(
            CREATE_JOINT_COMMAND_NAME
        )

        self.execute_plugin_command(
            CREATE_FK_COMMAND_NAME
        )

    def execute_plugin_command(self, command_name):
        command = getattr(
            cmds,
            command_name,
            None
        )

        if command is None:
            raise RuntimeError(
                "Maya command is not registered: {}".format(
                    command_name
                )
            )

        command(
            **self.current_command_arguments()
        )

    # ------------------------------------------------------------------
    # Error handling
    # ------------------------------------------------------------------

    def run_build_action(
        self,
        action,
        error_title
    ):
        try:
            self.ensure_plugin_loaded()
            action()

        except Exception as error:
            self.show_error(
                error_title,
                error
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
                "Could not load {}.\n\n{}".format(
                    PLUGIN_NAME,
                    error
                )
            )

    def show_error(
        self,
        title,
        error
    ):
        module_name = self.current_module()
        side_name = self.current_side()

        message = (
            "Module: {module}\n"
            "Side: {side}\n\n"
            "{error}"
        ).format(
            module=module_name,
            side=side_name,
            error=error
        )

        QtWidgets.QMessageBox.critical(
            self,
            title,
            message
        )


def show_panel():
    global _panel_instance

    workspace_control = (
        RigBuildPanel.OBJECT_NAME +
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

    _panel_instance = RigBuildPanel()

    _panel_instance.show(
        dockable=True,
        area="right",
        floating=True
    )

    return _panel_instance