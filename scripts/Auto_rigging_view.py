from PySide2 import QtWidgets

from maya.app.general.mayaMixin import MayaQWidgetDockableMixin


class RigBuildView(
    MayaQWidgetDockableMixin,
    QtWidgets.QWidget
):
    WINDOW_TITLE = "Auto Rigging"
    OBJECT_NAME = "MayaCppPluginRigBuildPanel"

    def __init__(
        self,
        module_names,
        parent=None
    ):
        super(RigBuildView, self).__init__(
            parent=parent
        )

        self.setObjectName(self.OBJECT_NAME)
        self.setWindowTitle(self.WINDOW_TITLE)
        self.setMinimumWidth(320)

        self.create_widgets(module_names)
        self.create_layout()

    # ------------------------------------------------------------------
    # Widget creation
    # ------------------------------------------------------------------

    def create_widgets(self, module_names):
        self.module_label = QtWidgets.QLabel(
            "Module"
        )

        self.module_combo_box = QtWidgets.QComboBox()
        self.module_combo_box.addItems(
            module_names
        )

        self.side_label = QtWidgets.QLabel(
            "Side"
        )

        self.side_combo_box = QtWidgets.QComboBox()

        self.create_guides_button = QtWidgets.QPushButton(
            "Create Locator Guides"
        )

        self.mirror_check_box = QtWidgets.QCheckBox(
            "Mirror"
        )
        self.mirror_check_box.setChecked(False)
        self.mirror_check_box.setToolTip(
            "Mirror the current locator guides across world X=0."
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

    # ------------------------------------------------------------------
    # Layout creation
    # ------------------------------------------------------------------

    def create_layout(self):
        selection_layout = QtWidgets.QFormLayout()

        selection_layout.setContentsMargins(
            0,
            0,
            0,
            0
        )
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

        build_options_layout.setContentsMargins(
            0,
            0,
            0,
            0
        )
        build_options_layout.setHorizontalSpacing(12)
        build_options_layout.setVerticalSpacing(8)

        build_options_layout.addRow(
            QtWidgets.QLabel("Build Options"),
            self.mirror_check_box
        )

        button_layout = QtWidgets.QVBoxLayout()
        button_layout.setSpacing(8)

        button_layout.addWidget(self.create_guides_button)

        button_layout.addWidget(self.create_joint_chain_button)

        button_layout.addWidget(self.create_fk_button)

        button_layout.addSpacing(8)

        button_layout.addWidget(self.build_all_button)

        main_layout = QtWidgets.QVBoxLayout(self)

        main_layout.setContentsMargins(
            12,
            12,
            12,
            12
        )
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