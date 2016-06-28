#!/usr/bin/env python
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import os
import sys
import random

""" Create custom HTTPRequestHandler class
"""
class KodeFunHTTPRequestHandler(BaseHTTPRequestHandler):

    # handle GET command
    def do_GET(self):
        try:
            # send code 200 response
            self.send_response(200)
            
            # send header first
            self.send_header('Content-type', 'text-html')
            self.end_headers()
            
            # random string
            resp_size = get_response_size()
            print resp_size
            response = 'a'*resp_size
            
            # send string to client
            self.wfile.write(response)
            return
        
        except IOError:
            self.send_error(404, 'file not found')

""" return response size in bytes
"""
def get_response_size():
    return int(sys.argv[1])
    sample = random.uniform(0, 1)
    
    if sample < 0.25:
        return 200
    elif sample < 0.5:
        return 1500
    elif sample < 0.75:
        return 5000
    else:
        return 50000    
    
def run():

    print('http server is starting...')
    
    #ip and port of server
    #by default http server port is 8000
    server_address = ('', 8000)
    httpd = HTTPServer(server_address, KodeFunHTTPRequestHandler)
    print('http server is running...')
    httpd.serve_forever()
    
if __name__ == '__main__':
    run()   # first argument is the ip address
