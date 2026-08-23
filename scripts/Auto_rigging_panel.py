from PySide2 import QtCore
from PySide2 import QtWidgets

import maya.cmds as cmds

from Auto_rigging_view import RigBuildView


PLUGIN_NAME = "MayaCppPlugin"

PRE_BUILD_COMMAND_NAME = "PreBuildBoneChain"
CREATE_JOINT_COMMAND_NAME = "CreateJointChain"
MIRROR_JOINT_COMMAND_NAME = "MirrorJointChain"
CREATE_FK_COMMAND_NAME = "CreateFkController"
CREATE_IK_COMMAND_NAME = "CreateIkController"


MODULE_ORDER = (
    "arm",
    "leg",
    "spine",
    "head",
)

MODULE_SIDES = {
    "arm": ("L", "R"),
    "leg": ("L", "R"),
    "spine": ("M",),
    "head": ("M",),
}


FULL_BODY_MODULES = (
    {
        "module": "spine",
        "side": "M",
        "fk": True,
        "ik": False,
        "mirror": False,
    },
    {
        "module": "head",
        "side": "M",
        "fk": True,
        "ik": False,
        "mirror": False,
    },
    {
        "module": "arm",
        "side": "L",
        "fk": True,
        "ik": True,
        "mirror": True,
    },
    {
        "module": "leg",
        "side": "L",
        "fk": True,
        "ik": True,
        "mirror": True,
    },
)


_panel_controller = None


class RigBuildController(QtCore.QObject):
    def __init__(self, parent=None):
        self.view = RigBuildView(
            module_names=MODULE_ORDER,
            parent=parent
        )

        super(RigBuildController, self).__init__(self.view)

        self.create_connections()
        self.update_module_options()

    # ------------------------------------------------------------------
    # Connections
    # ------------------------------------------------------------------

    def create_connections(self):
        self.view.module_combo_box.currentTextChanged.connect(
            self.update_module_options
        )

        self.view.create_full_body_guides_button.clicked.connect(
            self.create_full_body_guides
        )

        self.view.build_full_body_button.clicked.connect(
            self.build_full_body
        )

        self.view.create_module_guides_button.clicked.connect(
            self.create_module_guides
        )

        self.view.build_module_button.clicked.connect(
            self.build_module
        )

    # ------------------------------------------------------------------
    # Current module options
    # ------------------------------------------------------------------

    def current_module(self):
        return self.view.module_combo_box.currentText()

    def current_side(self):
        return self.view.side_combo_box.currentText()

    def fk_enabled(self):
        return self.view.fk_check_box.isChecked()

    def ik_enabled(self):
        return (
            self.view.ik_check_box.isEnabled()
            and self.view.ik_check_box.isChecked()
        )

    def mirror_enabled(self):
        return (
            self.view.mirror_check_box.isEnabled()
            and self.view.mirror_check_box.isChecked()
        )

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

    @QtCore.Slot(str)
    def update_module_options(self, _module_name=None):
        module_name = self.current_module()
        allowed_sides = MODULE_SIDES.get(
            module_name,
            ("M",)
        )

        side_combo_box = self.view.side_combo_box

        side_combo_box.blockSignals(True)
        side_combo_box.clear()
        side_combo_box.addItems(allowed_sides)

        if "L" in allowed_sides:
            side_combo_box.setCurrentText("L")
        else:
            side_combo_box.setCurrentIndex(0)

        side_combo_box.blockSignals(False)

        mirror_available = (
            module_name in ("arm", "leg")
            and "L" in allowed_sides
            and "R" in allowed_sides
        )

        self.view.mirror_check_box.setEnabled(mirror_available)

        if mirror_available:
            self.view.mirror_check_box.setToolTip(
                "Mirror the source joints using Maya Behavior mode."
            )
        else:
            self.view.mirror_check_box.setChecked(False)
            self.view.mirror_check_box.setToolTip(
                "Mirror is unavailable for center modules."
            )

        ik_available = module_name in ("arm", "leg")

        self.view.ik_check_box.setEnabled(ik_available)
        self.view.ik_check_box.setChecked(ik_available)

        if ik_available:
            self.view.ik_check_box.setToolTip("Build the IK joint chain and IK controllers.")
        else:
            self.view.ik_check_box.setToolTip("IK is currently available only for arm and leg.")

    # ------------------------------------------------------------------
    # Full-body buttons
    # ------------------------------------------------------------------

    @QtCore.Slot()
    def create_full_body_guides(self):
        self.run_build_action(
            self._create_full_body_guides,
            "Create Full Body Guides Failed"
        )

    @QtCore.Slot()
    def build_full_body(self):
        self.run_build_action(
            self._build_full_body,
            "Build Full Body Failed"
        )

    def _create_full_body_guides(self):
        for definition in FULL_BODY_MODULES:
            self.execute_plugin_command(
                PRE_BUILD_COMMAND_NAME,
                module=definition["module"],
                side=definition["side"]
            )

    def _build_full_body(self):
        # 第一阶段：创建所有源侧 Joint Chains。
        for definition in FULL_BODY_MODULES:
            self.create_source_joint_chains(
                module=definition["module"],
                side=definition["side"],
                build_fk=definition["fk"],
                build_ik=definition["ik"]
            )

        # 第二阶段：镜像 arm 和 leg 的 Joint Chains。
        for definition in FULL_BODY_MODULES:
            if not definition["mirror"]:
                continue

            self.mirror_joint_chains(
                module=definition["module"],
                source_side=definition["side"],
                mirror_fk=definition["fk"],
                mirror_ik=definition["ik"]
            )

        # 第三阶段：所有骨骼存在后创建控制器。
        for definition in FULL_BODY_MODULES:
            controller_sides = (definition["side"],)

            if definition["mirror"]:
                controller_sides = (
                    definition["side"],
                    self.opposite_side(definition["side"])
                )

            self.create_controllers(
                module=definition["module"],
                sides=controller_sides,
                build_fk=definition["fk"],
                build_ik=definition["ik"]
            )

    # ------------------------------------------------------------------
    # Module buttons
    # ------------------------------------------------------------------

    @QtCore.Slot()
    def create_module_guides(self):
        self.run_build_action(
            self._create_module_guides,
            "Create Module Guides Failed"
        )

    @QtCore.Slot()
    def build_module(self):
        self.run_build_action(
            self._build_module,
            "Build Module Failed"
        )

    def _create_module_guides(self):
        self.execute_plugin_command(
            PRE_BUILD_COMMAND_NAME,
            module=self.current_module(),
            side=self.current_side()
        )

    def _build_module(self):
        if not self.fk_enabled() and not self.ik_enabled():
            raise RuntimeError("Select at least one rig type: FK or IK.")

        module = self.current_module()
        source_side = self.current_side()

        self.create_source_joint_chains(
            module=module,
            side=source_side,
            build_fk=self.fk_enabled(),
            build_ik=self.ik_enabled()
        )

        controller_sides = (source_side,)

        if self.mirror_enabled():
            self.mirror_joint_chains(
                module=module,
                source_side=source_side,
                mirror_fk=self.fk_enabled(),
                mirror_ik=self.ik_enabled()
            )

            controller_sides = (
                source_side,
                self.opposite_side(source_side)
            )

        self.create_controllers(
            module=module,
            sides=controller_sides,
            build_fk=self.fk_enabled(),
            build_ik=self.ik_enabled()
        )

    # ------------------------------------------------------------------
    # Shared build operations
    # ------------------------------------------------------------------

    def create_source_joint_chains(
        self,
        module,
        side,
        build_fk,
        build_ik
    ):
        if build_fk:
            self.execute_plugin_command(
                CREATE_JOINT_COMMAND_NAME,
                module=module,
                side=side,
                chainType="fk"
            )

        if build_ik:
            self.execute_plugin_command(
                CREATE_JOINT_COMMAND_NAME,
                module=module,
                side=side,
                chainType="ik"
            )

    def mirror_joint_chains(self, module, source_side, mirror_fk, mirror_ik):
        if mirror_fk:
            self.execute_plugin_command(
                MIRROR_JOINT_COMMAND_NAME,
                module=module,
                side=source_side,
                chainType="fk"
            )

        if mirror_ik:
            self.execute_plugin_command(
                MIRROR_JOINT_COMMAND_NAME,
                module=module,
                side=source_side,
                chainType="ik"
            )

    def create_controllers(self, module, sides, build_fk, build_ik):
        for side in sides:
            if build_fk:
                self.execute_plugin_command(
                    CREATE_FK_COMMAND_NAME,
                    module=module,
                    side=side
                )

            if build_ik:
                self.execute_plugin_command(
                    CREATE_IK_COMMAND_NAME,
                    module=module,
                    side=side
                )

    # ------------------------------------------------------------------
    # Maya command execution
    # ------------------------------------------------------------------

    def execute_plugin_command(self, command_name, module, side, **extra_arguments):
        command = getattr(cmds, command_name, None)

        if command is None:
            raise RuntimeError("Maya command is not registered: {}".format(command_name))

        arguments = {
            "module": module,
            "side": side,
        }

        arguments.update(extra_arguments)

        return command(**arguments)

    # ------------------------------------------------------------------
    # Error handling
    # ------------------------------------------------------------------

    def run_build_action(self, action, error_title):
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
            self.show_error(error_title, error)

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
            raise RuntimeError("Could not load {}.\n\n{}".format(PLUGIN_NAME, error))

    def show_error(self, title, error):
        QtWidgets.QMessageBox.critical(
            self.view,
            title,
            str(error)
        )

    # ------------------------------------------------------------------
    # View lifecycle
    # ------------------------------------------------------------------

    def show(self):
        self.view.show(
            dockable=True,
            area="right",
            floating=True
        )

    def close(self):
        self.view.close()
        self.view.deleteLater()


def show_panel():
    global _panel_controller

    workspace_control = (RigBuildView.OBJECT_NAME + "WorkspaceControl")

    if cmds.workspaceControl(
        workspace_control,
        query=True,
        exists=True
    ):
        cmds.deleteUI(workspace_control)

    if _panel_controller is not None:
        try:
            _panel_controller.close()
        except RuntimeError:
            pass

    _panel_controller = RigBuildController()
    _panel_controller.show()

    return _panel_controller