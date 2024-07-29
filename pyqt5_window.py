from PyQt5 import QtWidgets
from PyQt5.QtWidgets import QApplication, QWidget
import sys

app = QApplication(sys.argv)
window = QWidget()
#window.setGeometry(400,400,300,300)
window.setWindowTitle("CodersLegacy")

date = QtWidgets.QTextEdit(window)
date.setPlainText("Preferably we need to let the compiler know that crash() does not return. If that's not easy (it's a macro, after all) then try using __builtin_unreachable here.")

window.show()
window.resize(date.width(), date.height())
sys.exit(app.exec_())
