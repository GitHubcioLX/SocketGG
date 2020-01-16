import sys
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect(('192.168.0.104', 1235))

msg = 'testttttt'
sock.send(bytes(msg, "utf-8"))

msg = sock.recv(64)
msg = msg.decode("utf-8")
print(msg)

msg = sock.recv(9)
msg = msg.decode("utf-8")
print(msg)
