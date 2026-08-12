from PySide2 import QtCore
from PySide2 import QtWidgets

import maya.cmds as cmds

from Auto_rigging_view import RigBuildView


PLUGIN_NAME = "MayaCppPlugin"

PRE_BUILD_COMMAND_NAME = "PreBuildBoneChain"
CREATE_JOINT_COMMAND_NAME = "CreateJointChain"
CREATE_FK_COMMAND_NAME = "CreateFkController"

# Module management
MODULE_ORDER = (
    "arm",
    "leg",
    "spine",
)

MODULE_SIDES = {
    "arm": ("L", "R"),
    "leg": ("L", "R"),
    "spine": ("M",),
}


_panel_controller = None


class RigBuildController(QtCore.QObject):

    def __init__(self, parent=None):
        self.view = RigBuildView(
            module_names=MODULE_ORDER,
            parent=parent
        )

        # Controller 以 View 为 QObject parent，
        # 防止 Controller 被 Python 垃圾回收。
        super(RigBuildController, self).__init__(self.view)

        self.create_connections()
        self.update_side_options()

    # ------------------------------------------------------------------
    # UI connections
    # ------------------------------------------------------------------

    def create_connections(self):
        self.view.module_combo_box.currentTextChanged.connect(self.update_side_options)

        self.view.create_guides_button.clicked.connect(self.create_locator_guides)

        self.view.create_joint_chain_button.clicked.connect(self.create_joint_chain)

        self.view.create_fk_button.clicked.connect(self.create_fk_controllers)

        self.view.build_all_button.clicked.connect(self.build_joint_chain_and_fk)

    # ------------------------------------------------------------------
    # Current UI state
    # ------------------------------------------------------------------

    def current_module(self):
        return self.view.module_combo_box.currentText()

    def current_side(self):
        return self.view.side_combo_box.currentText()

    def mirror_enabled(self):
        return (
            self.view.mirror_check_box.isEnabled()
            and self.view.mirror_check_box.isChecked()
        )

    def command_arguments(self, side=None):
        return {
            "module": self.current_module(),
            "side": side or self.current_side(),
        }

    @QtCore.Slot(str)
    def update_side_options(self, _module_name=None):
        module_name = self.current_module()

        allowed_sides = MODULE_SIDES.get(module_name,("M",))

        side_combo_box = (self.view.side_combo_box)

        side_combo_box.blockSignals(True)
        side_combo_box.clear()
        side_combo_box.addItems(allowed_sides)

        # Default : L
        if "L" in allowed_sides:
            side_combo_box.setCurrentText("L")
        else:
            side_combo_box.setCurrentIndex(0)

        side_combo_box.blockSignals(False)

        can_mirror = ("L" in allowed_sides and "R" in allowed_sides)

        self.view.mirror_check_box.setEnabled(can_mirror)

        if can_mirror:
            self.view.mirror_check_box.setToolTip(
                "Build the current side and its mirrored side."
            )
        else:
            self.view.mirror_check_box.setChecked(False)
            self.view.mirror_check_box.setToolTip(
                "Mirror is unavailable for center modules."
            )

    # ------------------------------------------------------------------
    # Guide name helpers
    # ------------------------------------------------------------------

    @staticmethod
    def opposite_side(side):
        opposite_sides = {
            "L": "R",
            "R": "L",
        }

        try:
            return opposite_sides[side]

        except KeyError:
            raise RuntimeError("Only L and R modules can be mirrored.")

    @staticmethod
    def module_prefix(module_name, side):
        return "{}_{}".format(side, module_name)

    def find_locator_guides(
        self,
        module_name,
        side
    ):
        prefix = self.module_prefix(module_name, side)

        pattern = "{}_*_guide".format(prefix)

        possible_guides = cmds.ls(
            pattern,
            type="transform",
            long=False
        ) or []

        locator_guides = []

        for transform_name in possible_guides:
            locator_shapes = cmds.listRelatives(
                transform_name,
                shapes=True,
                type="locator",
                fullPath=False
            ) or []

            if locator_shapes:
                locator_guides.append(transform_name)

        return sorted(locator_guides)

    # ------------------------------------------------------------------
    # Mirror implementation
    # ------------------------------------------------------------------

    def mirror_locator_guides(self):
        module_name = self.current_module()
        source_side = self.current_side()

        if source_side not in ("L", "R"):
            raise RuntimeError("The {} module cannot be mirrored.".format(module_name))

        target_side = self.opposite_side(source_side)

        source_guides = self.find_locator_guides(module_name, source_side)

        if not source_guides:
            raise RuntimeError(
                "No locator guides were found for {}_{}.\n\n"
                "Create and position the current side guides first.".format(source_side, module_name)
            )

        source_prefix = self.module_prefix(module_name, source_side)
        target_prefix = self.module_prefix(module_name, target_side)

        target_guide_names = [
            source_guide.replace(source_prefix, target_prefix, 1)
            for source_guide in source_guides
        ]

        existing_target_guides = [
            target_guide
            for target_guide in target_guide_names
            if cmds.objExists(target_guide)
        ]

        if not existing_target_guides:
            # 使用现有 C++ 命令创建目标侧 locator 和 guide curve。
            self.execute_plugin_command(
                PRE_BUILD_COMMAND_NAME,
                side=target_side
            )

        elif (
            len(existing_target_guides)
            != len(target_guide_names)
        ):
            missing_target_guides = [
                target_guide
                for target_guide in target_guide_names
                if not cmds.objExists(target_guide)
            ]

            raise RuntimeError(
                "The target-side guides are incomplete.\n\n"
                "Missing guides:\n{}".format("\n".join(missing_target_guides))
            )

        for source_guide, target_guide in zip(
            source_guides,
            target_guide_names
        ):
            if not cmds.objExists(target_guide):
                raise RuntimeError("The target guide was not created: {}".format(target_guide))

            source_position = cmds.xform(source_guide, query=True, worldSpace=True, translation=True)

            mirrored_position = (
                -source_position[0],
                source_position[1],
                source_position[2],
            )

            cmds.xform(target_guide, worldSpace=True, translation=mirrored_position)

        return (
            source_side,
            target_side,
        )

    def joint_build_sides(self):
        if not self.mirror_enabled():
            return (
                self.current_side(),
            )

        return self.mirror_locator_guides()

    def fk_build_sides(self):
        if not self.mirror_enabled():
            return (
                self.current_side(),
            )

        source_side = self.current_side()

        return (source_side,
            self.opposite_side(source_side),
        )

    # ------------------------------------------------------------------
    # Button handlers
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
    # Build operations
    # ------------------------------------------------------------------

    def _create_locator_guides(self):
        # 创建 guide 时只创建当前下拉菜单所选侧。
        # Mirror 只控制骨骼和 FK 构建。
        self.execute_plugin_command(PRE_BUILD_COMMAND_NAME, side=self.current_side())

    def _create_joint_chain(self):
        build_sides = self.joint_build_sides()

        for side in build_sides:
            self.execute_plugin_command(CREATE_JOINT_COMMAND_NAME, side=side)

    def _create_fk_controllers(self):
        build_sides = self.fk_build_sides()

        for side in build_sides:
            self.execute_plugin_command(CREATE_FK_COMMAND_NAME, side=side)

    def _build_joint_chain_and_fk(self):
        build_sides = self.joint_build_sides()

        for side in build_sides:
            self.execute_plugin_command(CREATE_JOINT_COMMAND_NAME, side=side)
        for side in build_sides:
            self.execute_plugin_command(CREATE_FK_COMMAND_NAME, side=side)

    # ------------------------------------------------------------------
    # Maya command execution
    # ------------------------------------------------------------------

    def execute_plugin_command(self, command_name, side=None):
        command = getattr(cmds, command_name, None)

        if command is None:
            raise RuntimeError("Maya command is not registered: {}".format(command_name))

        return command(**self.command_arguments(side=side))

    # ------------------------------------------------------------------
    # Error handling
    # ------------------------------------------------------------------

    def run_build_action(self, action, error_title):
        undo_chunk_open = False

        try:
            self.ensure_plugin_loaded()

            cmds.undoInfo(openChunk=True, chunkName="AutoRigBuild")
            undo_chunk_open = True

            action()

        except Exception as error:
            self.show_error(error_title, error)

        finally:
            if undo_chunk_open:
                cmds.undoInfo(closeChunk=True)

    def ensure_plugin_loaded(self):
        if cmds.pluginInfo(PLUGIN_NAME, query=True, loaded=True):
            return

        try:
            cmds.loadPlugin(PLUGIN_NAME)

        except RuntimeError as error:
            raise RuntimeError("Could not load {}.\n\n{}".format(PLUGIN_NAME, error))

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
            mirror=(
                "On"
                if self.mirror_enabled()
                else "Off"
            ),
            error=error
        )

        QtWidgets.QMessageBox.critical(self.view, title, message)

    # ------------------------------------------------------------------
    # View lifecycle
    # ------------------------------------------------------------------

    def show(self):
        self.view.show(dockable=True, area="right",floating=True)

    def close(self):
        self.view.close()
        self.view.deleteLater()