import socket
import time

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.0.101', 1237))

while True:
    flag = input("Co chcesz zrobic? 1- nowy, 2-wiadomosc, 0- logout: ")
    num = ""
    if flag != '0':
        num = input("Numer: ")
    msg = ""
    if flag == '2':
        msg = input("Wpisz tresc wiadomosci: ")
        msg = ';' + msg
    cat = flag + num + msg
    sock.send(bytes(cat, "utf-8"))

    msg = sock.recv(64)
    msg = msg.decode("utf-8")
    print(msg)

    #klient sluchajacy
    #msg = sock.recv(64)
    #msg = msg.decode("utf-8")
    #print(msg)

    time.sleep(0.2)
