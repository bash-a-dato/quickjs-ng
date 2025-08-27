#!/usr/bin/env python3
import socket
import time
import sys

def test_connection(host='127.0.0.1', port=56746):
    print(f"Attempting to connect to {host}:{port}")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # 5 second timeout
        
        print("Connecting...")
        sock.connect((host, port))
        print("Connected successfully!")
        
        # Send a simple message
        message = "Hello from test client\n"
        print(f"Sending: {message.strip()}")
        sock.send(message.encode())
        
        # Try to receive response
        print("Waiting for response...")
        response = sock.recv(1024)
        print(f"Received: {response}")
        
        sock.close()
        print("Connection closed")
        
    except socket.timeout:
        print("Connection timed out")
    except ConnectionRefusedError:
        print("Connection refused - is the server running?")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
        test_connection(port=port)
    else:
        print("Usage: python test_socket.py <port>")
        print("Example: python test_socket.py 56746")
