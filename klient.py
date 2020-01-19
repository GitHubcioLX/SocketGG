from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from threading import Thread, Lock
from time import sleep
import socket
import emoji

# Polaczenie sie z serwerem poprzez gniazdo
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.0.26', 1237))

# GUI
app = QApplication([])

# Okno czatu
text_area = QPlainTextEdit()
text_area.setFocusPolicy(Qt.NoFocus)
message = QLineEdit()

layout = QVBoxLayout()
layout.addWidget(text_area)
layout.addWidget(message)

window = QWidget()
window.setWindowTitle("Czat")
window.setLayout(layout)


# Okno logowania
label = QLabel()
label.setText("Podaj swoj numer:")
login_number = QLineEdit()
login_button = QPushButton("Zaloguj")

login_layout = QVBoxLayout()
login_layout.addWidget(label)
login_layout.addWidget(login_number)
login_layout.addWidget(login_button)

login_window = QWidget()
login_window.setWindowTitle("Logowanie")
login_window.setLayout(login_layout)


# Menu glowne
logged_label = QLabel()
logged_label.setAlignment(Qt.AlignRight)
chatters_label = QLabel()
chatters_label.setText("Aktywne rozmowy:")
chatters_list = QListWidget()
gap = QLabel()
new_label = QLabel()
new_label.setText("Podaj numer adresata:")
new_chatter = QLineEdit()
new_button = QPushButton("Otworz czat")

main_layout = QVBoxLayout()
main_layout.addWidget(logged_label)
main_layout.addWidget(chatters_label)
main_layout.addWidget(chatters_list)
main_layout.addWidget(gap)
main_layout.addWidget(new_label)
main_layout.addWidget(new_chatter)
main_layout.addWidget(new_button)

main_window = QWidget()
main_window.setWindowTitle("SocketGG")
main_window.setLayout(main_layout)


# Inicjalizacja zmiennych globalnych
new_messages = {}
chat_history = {}
chatters = []
chatter = ""
new_messages["$"] = []
new_messages[chatter] = []
chat_history[chatter] = []
login = ""
mutex = Lock()


# Uzupelnianie listy aktywnych konwersacji
def fill_up_list():
    global chatters, new_messages, chatters_list

    chatters_list.clear()
    for one in chatters:
        if new_messages[one]:
            item = one + " " + emoji.emojize(':bell:')
        else:
            item = one
        chatters_list.addItem(item)


# Obsluga listy aktywnych konwersacji
def refresh_list():
    global chatters, new_messages, chatters_list
    # Dodawanie nowych pozycji na liscie aktywnych rozmow
    if len(chatters) != chatters_list.count():
        fill_up_list()
    # Zmiana statusow rozmow (dzwoneczek)
    else:
        for i, one in enumerate(chatters):
            if new_messages[one]:
                x = one + " " + emoji.emojize(':bell:')
                y = chatters_list.item(i).text()
                if x != y:
                    chatters_list.item(i).setText(x)
            else:
                x = one
                y = chatters_list.item(i).text()
                if x != y:
                    chatters_list.item(i).setText(x)


# Obsluga wiadomosci odebranych od serwera
def fetch_new_messages():
    global new_messages, chatters, sock

    while True:
        response = sock.recv(128)
        response = response.decode("utf-8")
        if ';' in response:
            [sender, new_message] = response.split(';', 1)
            # Dodaj rozmowce jako element slownika new_messages
            if sender not in new_messages.keys():
                mutex.acquire()
                try:
                    new_messages[sender] = []
                finally:
                    mutex.release()
            # Dodaj nowego rozmowce, jesli do niego nie pisalismy
            if sender not in chatters and sender != "$":
                chatters.append(sender)
                chat_history[sender] = []
            mutex.acquire()
            try:
                new_messages[sender].append(new_message)
            finally:
                mutex.release()
        sleep(.5)


# Inicjalizacja watku odbierajacego nowe wiadomosci od serwera
thread = Thread(target=fetch_new_messages, daemon=True)
thread.start()


# Wyswietlanie nowych wiadomosci w oknie czatu
def display_new_messages():
    global chatter, window, new_messages, text_area, chat_history

    while window.isVisible() and (new_messages[chatter] or new_messages["$"]):
        mutex.acquire()
        # Wiadomosci systemowe
        if new_messages["$"]:
            try:
                text_area.appendPlainText("Error: " + new_messages["$"].pop(0))
            finally:
                mutex.release()
        # Wiadomosci uzytkownikow
        else:
            try:
                temp = chatter + ": " + new_messages[chatter].pop(0)
            finally:
                mutex.release()
            text_area.appendPlainText(temp)
            chat_history[chatter].append(temp)


# Wyswietlanie historii czatu
def display_chat_history():
    global chat_history, text_area

    for msg in chat_history[chatter]:
        text_area.appendPlainText(msg)


# Wyswietlanie okna czatu
def open_chat(number):
    global chatter, window, text_area, message, new_chatter

    window.hide()
    text_area.clear()
    message.clear()
    chatter = number
    new_chatter.clear()
    window.show()
    text_area.appendPlainText("Zaczales czat z numerem " + chatter + ".")
    display_chat_history()


# Otwarcie czatu z nowym rozmowca poprzez wpisanie numeru
def open_chat_new():
    global new_chatter, new_messages, chatters, chat_history

    if new_chatter.text().isnumeric():
        temp = new_chatter.text()
        if temp not in new_messages.keys():
            new_messages[temp] = []
        if temp not in chatters and temp != "$":
            chatters.append(temp)
            chat_history[temp] = []
        open_chat(temp)
    elif new_chatter.text():
        print("Numer powinien skladac sie tylko z cyfr.")


# Otwarcie czatu z nowym rozmowca poprzez liste aktywnych rozmow
def open_chat_list(item):
    if emoji.emojize(':bell:') in item.text():
        [new, _] = item.text().split(' ')
    else:
        new = item.text()

    open_chat(new)


# Obsluga nowej wiadomosci wychodzacej
def send_message():
    global chatter, login, sock, chat_history, message

    if len(message.text()) <= 110:
        sock.send(bytes('2' + chatter + ';' + message.text(), "utf-8"))
        chat_history[chatter].append(login + ": " + message.text())
        text_area.appendPlainText(login + ": " + message.text())
        message.clear()
    else:
        text_area.appendPlainText("Error: Wiadomosc jest za dluga.")


# Logowanie
def log_in():
    global login, sock, logged_label, login_window, main_window

    if login_number.text().isnumeric() and len(login_number.text()) <= 16:
        sock.send(bytes('1' + login_number.text(), "utf-8"))
        login = login_number.text()
        login_window.hide()
        logged_label.setText("Zalogowano jako <b>" + login + "<\b>")
        main_window.show()
    else:
        print("Numer powinien skladac z maksymalnie 16 cyfr.")


# Wyswietlenie okna logowania
login_window.show()

# Laczenie elementow interfejsu z ich funkcjami
login_button.clicked.connect(log_in)
login_number.returnPressed.connect(log_in)
new_button.clicked.connect(open_chat_new)
new_chatter.returnPressed.connect(open_chat_new)
chatters_list.itemClicked.connect(open_chat_list)
message.returnPressed.connect(send_message)

# Wlaczenie automatycznego uruchamiania funkcji co okreslony interwal
timer = QTimer()
timer.timeout.connect(display_new_messages)
timer.start(300)
list_timer = QTimer()
list_timer.timeout.connect(refresh_list)
list_timer.start(200)

# Uruchomienie aplikacji
app.exec_()

# Wylogowanie uzytkownika
sock.send(bytes('0', "utf-8"))
