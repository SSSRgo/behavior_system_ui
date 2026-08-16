from __future__ import annotations

from PySide6.QtWidgets import (
    QDoubleSpinBox,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from behavior_system.modules.led import LEDModule
from behavior_system.modules.lick import LickModule
from behavior_system.modules.motor import MotorModule
from behavior_system.modules.water import WaterModule


class MotorWidget(QWidget):
    def __init__(self, module: MotorModule) -> None:
        super().__init__()
        self.module = module

        self.rpm = QDoubleSpinBox()
        self.rpm.setRange(0.1, 600.0)
        self.rpm.setDecimals(1)
        self.rpm.setSingleStep(10.0)
        self.rpm.setValue(60.0)

        self.revolutions = QDoubleSpinBox()
        self.revolutions.setRange(-1000.0, 1000.0)
        self.revolutions.setDecimals(3)
        self.revolutions.setSingleStep(0.25)
        self.revolutions.setValue(1.0)

        self.degrees = QDoubleSpinBox()
        self.degrees.setRange(-3600.0, 3600.0)
        self.degrees.setDecimals(3)
        self.degrees.setSingleStep(5.0)
        self.degrees.setValue(60.0)
        self.degrees.setSuffix(" deg")

        set_rpm = QPushButton("Set RPM")
        set_rpm.clicked.connect(lambda: self.module.set_rpm(self.rpm.value()))

        move_custom = QPushButton("Move Revolutions")
        move_custom.clicked.connect(lambda: self.module.move_revolutions(self.revolutions.value()))

        move_degrees = QPushButton("Move Degrees")
        move_degrees.clicked.connect(lambda: self.module.move_degrees(self.degrees.value()))

        forward = QPushButton("+1 Rev")
        forward.clicked.connect(self.module.move_forward_one_rev)

        reverse = QPushButton("-1 Rev")
        reverse.clicked.connect(self.module.move_reverse_one_rev)

        swing_once = QPushButton("120 deg Once")
        swing_once.clicked.connect(self.module.swing_120_once)

        swing_repeat = QPushButton("Repeat 120 deg")
        swing_repeat.clicked.connect(self.module.start_swing_120)

        stop_swing = QPushButton("Stop Repeat")
        stop_swing.clicked.connect(self.module.stop_swing_120)

        enable = QPushButton("Toggle Enable")
        enable.clicked.connect(self.module.toggle_enable)

        configure = QPushButton("Configure Driver")
        configure.clicked.connect(self.module.configure_driver)

        form = QFormLayout()
        form.addRow("RPM", self.rpm)
        form.addRow("Revolutions", self.revolutions)
        form.addRow("Degrees", self.degrees)

        row1 = QHBoxLayout()
        row1.addWidget(set_rpm)
        row1.addWidget(move_custom)
        row1.addWidget(move_degrees)
        row1.addWidget(forward)
        row1.addWidget(reverse)

        row2 = QHBoxLayout()
        row2.addWidget(swing_once)
        row2.addWidget(swing_repeat)
        row2.addWidget(stop_swing)

        row3 = QHBoxLayout()
        row3.addWidget(enable)
        row3.addWidget(configure)
        row3.addStretch(1)

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addLayout(row1)
        layout.addLayout(row2)
        layout.addLayout(row3)
        layout.addStretch(1)


class LickWidget(QWidget):
    def __init__(self, module: LickModule) -> None:
        super().__init__()
        self.module = module

        arm = QPushButton("Arm")
        arm.clicked.connect(self.module.arm)

        disarm = QPushButton("Disarm")
        disarm.clicked.connect(self.module.disarm)

        simulate = QPushButton("Simulate Lick")
        simulate.clicked.connect(self.module.simulate_lick)

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Future lickometer controls will live here."))
        controls = QHBoxLayout()
        controls.addWidget(arm)
        controls.addWidget(disarm)
        controls.addWidget(simulate)
        controls.addStretch(1)
        layout.addLayout(controls)
        layout.addStretch(1)


class LEDWidget(QWidget):
    def __init__(self, module: LEDModule) -> None:
        super().__init__()
        self.module = module

        self.duration = QSpinBox()
        self.duration.setRange(1, 60000)
        self.duration.setValue(100)
        self.duration.setSuffix(" ms")

        pulse = QPushButton("Pulse")
        pulse.clicked.connect(lambda: self.module.pulse(self.duration.value()))

        box = QGroupBox("LED pulse")
        form = QFormLayout(box)
        form.addRow("Duration", self.duration)
        form.addRow(pulse)

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Future LED hardware command mapping goes in LEDModule."))
        layout.addWidget(box)
        layout.addStretch(1)


class WaterWidget(QWidget):
    def __init__(self, module: WaterModule) -> None:
        super().__init__()
        self.module = module

        self.duration = QSpinBox()
        self.duration.setRange(1, 10000)
        self.duration.setValue(20)
        self.duration.setSuffix(" ms")

        reward = QPushButton("Reward")
        reward.clicked.connect(lambda: self.module.reward(self.duration.value()))

        box = QGroupBox("Water reward")
        form = QFormLayout(box)
        form.addRow("Valve open", self.duration)
        form.addRow(reward)

        layout = QVBoxLayout(self)
        layout.addWidget(QLabel("Future valve command mapping goes in WaterModule."))
        layout.addWidget(box)
        layout.addStretch(1)
