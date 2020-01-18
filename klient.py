from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from requests import Session
from threading import Thread
from time import sleep
import socket
import time

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.0.21', 1237))

# GUI:
app = QApplication([])
text_area = QPlainTextEdit()
text_area.setFocusPolicy(Qt.NoFocus)
message = QLineEdit()
layout = QVBoxLayout()
layout.addWidget(text_area)
layout.addWidget(message)
window = QWidget()
window.setLayout(layout)
window.show()

# Event handlers:
new_messages = []
logged = False
chatter = ""


def fetch_new_messages():
    while True:
        response = sock.recv(64)
        response = response.decode("utf-8")
        if response:
            new_messages.append(response)
        sleep(.5)


thread = Thread(target=fetch_new_messages, daemon=True)
thread.start()


def display_new_messages():
    while new_messages:
        text_area.appendPlainText(new_messages.pop(0))


def send_message():
    global logged, chatter

    if not logged:
        sock.send(bytes('1' + message.text(), "utf-8"))
        logged = True
        text_area.appendPlainText("Zalogowano. Podaj numer adresata")
    elif chatter is "":
        chatter = message.text()
        text_area.appendPlainText("Zaczales czat z uzytkownikiem " + chatter)
    else:
        sock.send(bytes('2' + chatter + ';' + message.text(), "utf-8"))

    message.clear()


text_area.appendPlainText("Podaj swoj numer uzytkownika")
message.returnPressed.connect(send_message)
timer = QTimer()
timer.timeout.connect(display_new_messages)
timer.start(1000)

app.exec_()
