import socket

SERVER_IP = '192.168.1.4'
PORT_NUM = 53211
SIZE = 2048

self_connect = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
connect_pi = socket.socket(socket.AF_INET, socket.SOCK_STREAM)


def get_ip_address(sock):
    sock.connect(('8.8.8.8', 80))
    ip_address = self_connect.getsockname()[0]
    return ip_address

def close_socket(sock):
    sock.close()

def connect(address, sock):
    sock.connect( (address, PORT_NUM) )

if __name__ == '__main__':
    CURRENT_IP = get_ip_address(self_connect)
    print("Current IP : {}".format(CURRENT_IP))
    connect(SERVER_IP, connect_pi)
    connect_pi.send( "Hello" )
    print( connect_pi.recv(1024) )



