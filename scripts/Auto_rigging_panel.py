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

MODULE_BONES = {
    "arm": (
        "shoulder",
        "elbow",
        "wrist",
    ),
    "leg": (
        "thigh",
        "knee",
        "ankle",
        "ball",
        "toe",
    ),
    "spine": (
        "pelvis",
        "spine_01",
        "spine_02",
        "chest",
    ),
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
        self.setMinimumWidth(320)

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

        self.mirror_check_box = QtWidgets.QCheckBox(
            "Mirror"
        )
        self.mirror_check_box.setChecked(False)
        self.mirror_check_box.setToolTip(
            "Mirror the current locator guides across world X=0, "
            "then build both L and R sides."
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

        build_options_layout = QtWidgets.QFormLayout()
        build_options_layout.setContentsMargins(0, 0, 0, 0)
        build_options_layout.setHorizontalSpacing(12)
        build_options_layout.setVerticalSpacing(8)

        build_options_layout.addRow(
            QtWidgets.QLabel("Build Options"),
            self.mirror_check_box
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
        main_layout.addLayout(build_options_layout)
        main_layout.addWidget(self.create_separator())
        main_layout.addLayout(button_layout)
        main_layout.addStretch()

    @staticmethod
    def create_separator():
        separator = QtWidgets.QFrame()
        separator.setFrameShape(QtWidgets.QFrame.HLine)
        separator.setFrameShadow(QtWidgets.QFrame.Sunken)
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

    def mirror_enabled(self):
        return (
            self.mirror_check_box.isEnabled()
            and self.mirror_check_box.isChecked()
        )

    def current_command_arguments(self, side=None):
        return {
            "module": self.current_module(),
            "side": side or self.current_side(),
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
            self.side_combo_box.setCurrentText(previous_side)
        elif "L" in allowed_sides:
            # 所有左右侧模块默认选择 L。
            self.side_combo_box.setCurrentText("L")
        else:
            self.side_combo_box.setCurrentIndex(0)

        self.side_combo_box.blockSignals(False)

        can_mirror = (
            "L" in allowed_sides
            and "R" in allowed_sides
        )

        self.mirror_check_box.setEnabled(can_mirror)

        if not can_mirror:
            self.mirror_check_box.setChecked(False)
            self.mirror_check_box.setToolTip(
                "Mirror is unavailable for center modules."
            )
        else:
            self.mirror_check_box.setToolTip(
                "Mirror the current locator guides across world X=0, "
                "then build both L and R sides."
            )

    # ------------------------------------------------------------------
    # Name helpers
    # ------------------------------------------------------------------

    @staticmethod
    def opposite_side(side):
        if side == "L":
            return "R"

        if side == "R":
            return "L"

        raise RuntimeError(
            "Only L and R modules can be mirrored."
        )

    @staticmethod
    def module_prefix(module_name, side):
        return "{}_{}".format(
            side,
            module_name
        )

    def guide_names(self, module_name, side):
        try:
            bone_labels = MODULE_BONES[module_name]
        except KeyError:
            raise RuntimeError(
                "No guide definition found for module: {}".format(
                    module_name
                )
            )

        prefix = self.module_prefix(
            module_name,
            side
        )

        return [
            "{}_{}_guide".format(
                prefix,
                bone_label
            )
            for bone_label in bone_labels
        ]

    # ------------------------------------------------------------------
    # Mirror guide handling
    # ------------------------------------------------------------------

    def validate_source_guides(
        self,
        module_name,
        source_side
    ):
        source_guides = self.guide_names(
            module_name,
            source_side
        )

        missing_guides = [
            guide_name
            for guide_name in source_guides
            if not cmds.objExists(guide_name)
        ]

        if missing_guides:
            raise RuntimeError(
                "The current side locator guides are incomplete.\n\n"
                "Missing:\n{}".format(
                    "\n".join(missing_guides)
                )
            )

        return source_guides

    def ensure_target_guides(
        self,
        module_name,
        target_side
    ):
        target_guides = self.guide_names(
            module_name,
            target_side
        )

        existing_guides = [
            guide_name
            for guide_name in target_guides
            if cmds.objExists(guide_name)
        ]

        if not existing_guides:
            # 利用现有 C++ 命令创建目标侧 locator 和 guide curve，
            # 随后再覆盖 locator 的世界坐标。
            self.execute_plugin_command(
                PRE_BUILD_COMMAND_NAME,
                side=target_side
            )

        elif len(existing_guides) != len(target_guides):
            missing_guides = [
                guide_name
                for guide_name in target_guides
                if not cmds.objExists(guide_name)
            ]

            raise RuntimeError(
                "The mirrored side has incomplete locator guides.\n\n"
                "Delete or repair that side before mirroring.\n\n"
                "Missing:\n{}".format(
                    "\n".join(missing_guides)
                )
            )

        return target_guides

    def mirror_locator_guides(self):
        module_name = self.current_module()
        source_side = self.current_side()

        if source_side not in ("L", "R"):
            raise RuntimeError(
                "The {} module cannot be mirrored.".format(
                    module_name
                )
            )

        target_side = self.opposite_side(
            source_side
        )

        source_guides = self.validate_source_guides(
            module_name,
            source_side
        )

        target_guides = self.ensure_target_guides(
            module_name,
            target_side
        )

        for source_guide, target_guide in zip(
            source_guides,
            target_guides
        ):
            source_position = cmds.xform(
                source_guide,
                query=True,
                worldSpace=True,
                translation=True
            )

            mirrored_position = (
                -source_position[0],
                source_position[1],
                source_position[2],
            )

            cmds.xform(
                target_guide,
                worldSpace=True,
                translation=mirrored_position
            )

        return source_side, target_side

    def build_sides(self):
        if not self.mirror_enabled():
            return (self.current_side(),)

        source_side, target_side = (
            self.mirror_locator_guides()
        )

        return (
            source_side,
            target_side,
        )

    # ------------------------------------------------------------------
    # Public button slots
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

    # ------------------------------------------------------------------
    # Build implementation
    # ------------------------------------------------------------------

    def _create_locator_guides(self):
        # Mirror 只控制骨骼和 FK 构建。
        # 创建 locator 时始终只创建下拉菜单中的当前侧。
        self.execute_plugin_command(
            PRE_BUILD_COMMAND_NAME,
            side=self.current_side()
        )

    def _create_joint_chain(self):
        sides = self.build_sides()

        for side in sides:
            self.execute_plugin_command(
                CREATE_JOINT_COMMAND_NAME,
                side=side
            )

    def _create_fk_controllers(self):
        sides = self.build_sides()

        for side in sides:
            self.execute_plugin_command(
                CREATE_FK_COMMAND_NAME,
                side=side
            )

    def _build_joint_chain_and_fk(self):
        sides = self.build_sides()

        # 先完成所有骨骼，再创建所有 FK 控制器。
        for side in sides:
            self.execute_plugin_command(
                CREATE_JOINT_COMMAND_NAME,
                side=side
            )

        for side in sides:
            self.execute_plugin_command(
                CREATE_FK_COMMAND_NAME,
                side=side
            )

    def execute_plugin_command(
        self,
        command_name,
        side=None
    ):
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

        return command(
            **self.current_command_arguments(
                side=side
            )
        )

    # ------------------------------------------------------------------
    # Error handling and undo grouping
    # ------------------------------------------------------------------

    def run_build_action(
        self,
        action,
        error_title
    ):
        undo_chunk_open = False

        try:
            self.ensure_plugin_loaded()

            cmds.undoInfo(
                openChunk=True,
                chunkName="AutoRigBuild"
            )
            undo_chunk_open = True

            action()

        except Exception as error:
            self.show_error(
                error_title,
                error
            )

        finally:
            if undo_chunk_open:
                cmds.undoInfo(closeChunk=True)

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
        message = (
            "Module: {module}\n"
            "Side: {side}\n"
            "Mirror: {mirror}\n\n"
            "{error}"
        ).format(
            module=self.current_module(),
            side=self.current_side(),
            mirror="On" if self.mirror_enabled() else "Off",
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
        RigBuildPanel.OBJECT_NAME
        + "WorkspaceControl"
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