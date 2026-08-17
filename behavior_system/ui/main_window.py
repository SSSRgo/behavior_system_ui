from __future__ import annotations

from pathlib import Path

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QFileDialog,
    QFormLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QPlainTextEdit,
    QSpinBox,
    QSplitter,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from behavior_system.core.event_bus import EventBus
from behavior_system.core.events import Event
from behavior_system.core.session import EventLogger, SessionConfig
from behavior_system.devices.serial_transport import MockTransport, NullTransport, SerialTransport, list_serial_ports
from behavior_system.devices.protocol import DeviceProtocolDecoder
from behavior_system.modules.led import LEDModule
from behavior_system.modules.lick import LickModule
from behavior_system.modules.motor import MotorModule
from behavior_system.modules.water import WaterModule
from behavior_system.tasks.config import TaskConfig, list_task_configs
from behavior_system.tasks.registry import TaskRegistry
from behavior_system.ui.module_widgets import LEDWidget, LickWidget, MotorWidget, WaterWidget
from behavior_system.ui.timeline_widget import TimelineWidget


PROJECT_ROOT = Path(__file__).resolve().parents[2]


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("Behavior Control System")

        self.bus = EventBus()
        self.bus.event_published.connect(self.on_event)

        self.transport = NullTransport()
        self.device_decoder = DeviceProtocolDecoder()
        self.logger = EventLogger(PROJECT_ROOT / "logs")

        self.motor = MotorModule(self.transport, self.bus)
        self.lick = LickModule(self.transport, self.bus)
        self.led = LEDModule(self.transport, self.bus)
        self.water = WaterModule(self.transport, self.bus)
        self.modules = [self.motor, self.lick, self.water, self.led]
        self.task_registry = TaskRegistry(self.motor, self.bus, lick=self.lick, water=self.water, led=self.led)
        self.current_task = None
        self.current_task_config: TaskConfig | None = None
        self.task_config_dir = PROJECT_ROOT / "configs" / "tasks"

        self.port_combo = QComboBox()
        self.baud_spin = QSpinBox()
        self.baud_spin.setRange(1200, 1000000)
        self.baud_spin.setValue(115200)
        self.mock_check = QCheckBox("Mock device")
        self.status_label = QLabel("Disconnected")

        self.animal_edit = QLineEdit()
        self.experimenter_edit = QLineEdit()
        self.task_edit = QLineEdit("motor_test")
        self.notes_edit = QPlainTextEdit()
        self.notes_edit.setMaximumHeight(90)

        self.event_log = QPlainTextEdit()
        self.event_log.setReadOnly(True)
        self.timeline = TimelineWidget(window_seconds=30)
        self.timeline_window_spin = QSpinBox()
        self.timeline_window_spin.setRange(5, 600)
        self.timeline_window_spin.setValue(30)
        self.timeline_window_spin.setSuffix(" s")
        self.timeline_window_spin.valueChanged.connect(self.timeline.set_window_seconds)

        self.task_config_combo = QComboBox()
        self.task_config_combo.currentIndexChanged.connect(lambda _index: self.load_selected_task_config())
        self.task_config_preview = QPlainTextEdit()
        self.task_config_preview.setReadOnly(True)
        self.task_config_preview.setMaximumHeight(150)
        self.task_status_label = QLabel("Stopped")

        self.build_ui()
        self.refresh_ports()
        self.refresh_task_configs()
        self.publish("app", "ready")

    def build_ui(self) -> None:
        root = QWidget()
        main_layout = QVBoxLayout(root)

        top_row = QHBoxLayout()
        top_row.addWidget(self.build_serial_panel(), 2)
        top_row.addWidget(self.build_session_panel(), 3)
        top_row.addWidget(self.build_task_panel(), 2)
        main_layout.addLayout(top_row)

        splitter = QSplitter(Qt.Horizontal)
        splitter.addWidget(self.build_module_tabs())
        splitter.addWidget(self.build_event_panel())
        splitter.setSizes([760, 420])
        main_layout.addWidget(splitter, 1)

        self.setCentralWidget(root)

    def build_serial_panel(self) -> QGroupBox:
        box = QGroupBox("Device")
        refresh = QPushButton("Refresh")
        refresh.clicked.connect(self.refresh_ports)

        connect = QPushButton("Connect")
        connect.clicked.connect(self.connect_device)

        disconnect = QPushButton("Disconnect")
        disconnect.clicked.connect(self.disconnect_device)

        row = QHBoxLayout()
        row.addWidget(refresh)
        row.addWidget(connect)
        row.addWidget(disconnect)

        form = QFormLayout(box)
        form.addRow("Port", self.port_combo)
        form.addRow("Baud", self.baud_spin)
        form.addRow(self.mock_check)
        form.addRow(row)
        form.addRow("Status", self.status_label)
        return box

    def build_session_panel(self) -> QGroupBox:
        box = QGroupBox("Session")
        start = QPushButton("Start Session")
        start.clicked.connect(self.start_session)

        stop = QPushButton("Stop Session")
        stop.clicked.connect(self.stop_session)

        row = QHBoxLayout()
        row.addWidget(start)
        row.addWidget(stop)
        row.addStretch(1)

        form = QFormLayout(box)
        form.addRow("Animal ID", self.animal_edit)
        form.addRow("Experimenter", self.experimenter_edit)
        form.addRow("Task", self.task_edit)
        form.addRow("Notes", self.notes_edit)
        form.addRow(row)
        return box

    def build_task_panel(self) -> QGroupBox:
        box = QGroupBox("Tasks")

        refresh = QPushButton("Refresh")
        refresh.clicked.connect(self.refresh_task_configs)

        browse = QPushButton("Browse")
        browse.clicked.connect(self.browse_task_config)

        load = QPushButton("Load")
        load.clicked.connect(self.load_selected_task_config)

        start = QPushButton("Start Task")
        start.clicked.connect(self.start_configured_task)

        stop = QPushButton("Stop Task")
        stop.clicked.connect(self.stop_all_tasks)

        file_row = QHBoxLayout()
        file_row.addWidget(self.task_config_combo, 1)
        file_row.addWidget(refresh)
        file_row.addWidget(browse)
        file_row.addWidget(load)

        run_row = QHBoxLayout()
        run_row.addWidget(start)
        run_row.addWidget(stop)
        run_row.addStretch(1)

        layout = QVBoxLayout(box)
        layout.addWidget(QLabel("Task file"))
        layout.addLayout(file_row)
        layout.addWidget(self.task_config_preview)
        layout.addLayout(run_row)
        layout.addWidget(self.task_status_label)
        return box

    def build_module_tabs(self) -> QTabWidget:
        tabs = QTabWidget()
        tabs.addTab(MotorWidget(self.motor), self.motor.info.display_name)
        tabs.addTab(LickWidget(self.lick), self.lick.info.display_name)
        tabs.addTab(WaterWidget(self.water), self.water.info.display_name)
        tabs.addTab(LEDWidget(self.led), self.led.info.display_name)
        return tabs

    def build_event_panel(self) -> QGroupBox:
        box = QGroupBox("Events")
        clear = QPushButton("Clear")
        clear.clicked.connect(self.clear_events)

        controls = QHBoxLayout()
        controls.addWidget(QLabel("Timeline"))
        controls.addWidget(self.timeline_window_spin)
        controls.addStretch(1)
        controls.addWidget(clear)

        layout = QVBoxLayout(box)
        layout.addLayout(controls)
        layout.addWidget(self.timeline, 2)
        layout.addWidget(self.event_log, 1)
        return box

    def refresh_ports(self) -> None:
        current = self.port_combo.currentText()
        self.port_combo.clear()
        ports = list_serial_ports()
        self.port_combo.addItems(ports)
        if current:
            index = self.port_combo.findText(current)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def refresh_task_configs(self) -> None:
        current = self.task_config_combo.currentData()
        self.task_config_combo.clear()
        task_files = []
        for path in list_task_configs(self.task_config_dir):
            try:
                if TaskConfig.from_file(path).task_type == "arduino_experiment":
                    task_files.append(path)
            except Exception:
                continue
        for path in task_files:
            label = f"{path.stem} ({path.suffix.lstrip('.')})"
            self.task_config_combo.addItem(label, str(path))

        if current:
            index = self.task_config_combo.findData(current)
            if index >= 0:
                self.task_config_combo.setCurrentIndex(index)

        if self.task_config_combo.count() > 0 and self.current_task_config is None:
            self.load_selected_task_config()

    def browse_task_config(self) -> None:
        path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Arduino experiment config",
            str(self.task_config_dir),
            "Arduino experiment config (*.json)",
        )
        if not path:
            return

        config_path = Path(path)
        index = self.task_config_combo.findData(str(config_path))
        if index < 0:
            self.task_config_combo.addItem(config_path.stem, str(config_path))
            index = self.task_config_combo.findData(str(config_path))
        self.task_config_combo.setCurrentIndex(index)
        self.load_selected_task_config()

    def load_selected_task_config(self) -> None:
        raw_path = self.task_config_combo.currentData()
        if not raw_path:
            self.task_config_preview.setPlainText("No task config found.")
            self.current_task_config = None
            return

        try:
            config = TaskConfig.from_file(Path(raw_path))
        except Exception as exc:
            QMessageBox.critical(self, "Task config error", str(exc))
            self.publish("task", "config_load_failed", path=str(raw_path), error=str(exc))
            return

        if config.task_type != "arduino_experiment":
            self.current_task_config = None
            self.task_config_preview.setPlainText(
                "Rejected: production tasks must use task_type 'arduino_experiment'."
            )
            QMessageBox.warning(
                self,
                "PC-timed task disabled",
                "Production tasks must use task_type 'arduino_experiment' so the Arduino owns timing.",
            )
            self.publish(
                "task",
                "non_realtime_config_rejected",
                name=config.name,
                task_type=config.task_type,
            )
            return

        self.current_task_config = config
        self.task_edit.setText(config.name)
        self.task_config_preview.setPlainText(config.preview_text())
        self.task_status_label.setText(f"Loaded: {config.name}")
        self.publish("task", "config_loaded", name=config.name, task_type=config.task_type, path=str(config.path))

    def connect_device(self) -> None:
        try:
            if self.mock_check.isChecked():
                transport = MockTransport(on_line=self.on_device_line)
                self.set_transport(transport)
                self.status_label.setText("Connected to mock device")
                self.publish("device", "connected", mode="mock")
                return

            port = self.port_combo.currentText().strip()
            if not port:
                QMessageBox.warning(self, "No port", "Select a serial port or enable Mock device.")
                return

            transport = SerialTransport(on_line=self.on_device_line)
            transport.open(port=port, baudrate=self.baud_spin.value())
            self.set_transport(transport)
            self.status_label.setText(f"Connected: {port}")
            self.publish("device", "connected", port=port, baudrate=self.baud_spin.value())
        except Exception as exc:
            QMessageBox.critical(self, "Connection failed", str(exc))
            self.publish("device", "connection_failed", error=str(exc))

    def disconnect_device(self) -> None:
        self.transport.close()
        self.set_transport(NullTransport())
        self.status_label.setText("Disconnected")
        self.publish("device", "disconnected")

    def set_transport(self, transport) -> None:
        self.transport = transport
        self.device_decoder = DeviceProtocolDecoder()
        for module in self.modules:
            module.set_transport(transport)

    def start_session(self) -> None:
        if self.logger.is_open:
            QMessageBox.information(self, "Session active", "A session is already running.")
            return

        config = SessionConfig(
            animal_id=self.animal_edit.text(),
            experimenter=self.experimenter_edit.text(),
            task_name=self.task_edit.text(),
            notes=self.notes_edit.toPlainText(),
            parameters={
                "serial_baud": self.baud_spin.value(),
                "modules": [module.info.name for module in self.modules],
                "task_config": self.current_task_config.as_dict() if self.current_task_config else None,
            },
        )
        session_dir = self.logger.start(config)
        self.publish("session", "started", session_dir=str(session_dir))

    def stop_session(self) -> None:
        if not self.logger.is_open:
            return
        self.stop_all_tasks()
        self.publish("session", "stopped")
        self.logger.stop()

    def start_configured_task(self) -> None:
        if self.current_task_config is None:
            self.load_selected_task_config()
        if self.current_task_config is None:
            QMessageBox.warning(self, "No task config", "Load a task config before starting.")
            return
        if self.current_task_config.task_type != "arduino_experiment":
            QMessageBox.warning(
                self,
                "PC-timed task disabled",
                "Use an Arduino experiment config; Windows timers are not allowed to schedule trials.",
            )
            return

        self.stop_all_tasks()
        try:
            task = self.task_registry.create(self.current_task_config)
            task.start_from_config(self.current_task_config.parameters)
        except Exception as exc:
            QMessageBox.critical(self, "Task start failed", str(exc))
            self.publish("task", "start_failed", error=str(exc), name=self.current_task_config.name)
            return

        self.current_task = task
        self.task_status_label.setText(f"Running: {self.current_task_config.name}")
        self.publish(
            "task",
            "started",
            name=self.current_task_config.name,
            task_type=self.current_task_config.task_type,
            parameters=self.current_task_config.parameters,
        )

    def stop_all_tasks(self) -> None:
        if self.current_task is not None:
            try:
                self.current_task.stop()
            finally:
                task_name = self.current_task_config.name if self.current_task_config else ""
                self.current_task = None
                self.task_status_label.setText("Stopped")
                self.publish("task", "stopped", name=task_name)

    def clear_events(self) -> None:
        self.event_log.clear()
        self.timeline.clear()

    def on_device_line(self, line: str) -> None:
        try:
            event = self.device_decoder.decode(line)
        except ValueError as exc:
            event = Event(
                source="device",
                event_type="PROTOCOL_ERROR",
                payload={"line": line, "error": str(exc)},
            )
        self.bus.publish(event)

    def publish(self, source: str, event_type: str, **payload) -> None:
        self.bus.publish(Event(source=source, event_type=event_type, payload=payload))

    def on_event(self, event: Event) -> None:
        payload = event.payload if event.payload else {}
        master = "-" if event.master_timestamp_us is None else str(event.master_timestamp_us)
        trial = "-" if event.trial_number is None else str(event.trial_number)
        self.event_log.appendPlainText(
            f"{event.timestamp} | master_us={master} | trial={trial} | "
            f"{event.source} | {event.event_type} | value={event.value} | {payload}"
        )
        self.timeline.add_event(event)
        self.logger.log(event)

    def closeEvent(self, event) -> None:
        self.stop_all_tasks()
        self.transport.close()
        self.logger.stop()
        event.accept()
