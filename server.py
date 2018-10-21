#!/usr/bin/env python

from http.server import BaseHTTPRequestHandler, HTTPServer
from string import Template
import json
# HTTPRequestHandler class


class Server(BaseHTTPRequestHandler):

    # GET
    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        html = open("./data/main.html", "r").read()
        html = html.replace('{{CONFIG}}', json.dumps(
            {
                "foo": "bar"
            }
        )).replace('{{AUTH_URL}}', "https://console.cloud.google.com")

        self.wfile.write(bytes(html, "utf8"))


def run():
    print('starting server...')

    # Server settings
    # Choose port 8080, for port 80, which is normally used for a http server, you need root access
    server_address = ('127.0.0.1', 8081)
    httpd = HTTPServer(server_address, Server)
    print('running server...')
    httpd.serve_forever()


run()
