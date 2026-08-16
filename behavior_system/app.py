import sys

from PySide6.QtWidgets import QApplication

from behavior_system.ui.main_window import MainWindow


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("Behavior Control System")
    app.setOrganizationName("Lab")

    window = MainWindow()
    window.resize(1280, 820)
    window.show()

    return app.exec()
