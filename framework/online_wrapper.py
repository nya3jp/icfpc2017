#!/usr/bin/python3

import argparse
import socket
import subprocess
import json
import logging

def _connect(host, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    logging.info('connecting: %s, %d', host, port)
    sock.connect((host, port))
    return sock


def _send(sock, data):
    content = json.dumps(data).encode('utf-8')
    content_length = len(content)
    data = str(content_length).encode('utf-8') + b':' + content
    logging.debug('Sending: %r', data)
    sock.send(data)


def _recv(sock):
    content = b''
    while True:
        data = sock.recv(1024)
        content += data
        if b':' in data:
            break
    logging.debug('Received: %r', content)
    content_length, content = content.split(b':', 1)
    content_length = int(content_length.decode('utf-8'))
    while True:
        # TODO
        remaining = content_length - len(content)
        if remaining <= 0:
            return json.loads(content.decode('utf-8'))
        data = sock.recv(remaining)
        logging.debug('Received: %r', data)
        content += data


def _run_subprocess(command, data):
    inputdata = json.dumps(data).encode('utf-8')
    inputdata = str(len(inputdata)).encode('utf-8') + b':' + inputdata
    output = subprocess.check_call(command, input=inputdata)
    return json.loads(output.decode('utf-8').split(':', 1)[1])


def _run(sock, name, command):
    # Handshake
    _send(sock, {'me': name})
    response = _recv(sock)
    if response['you'] != name:
        raise Exception(json.dumps(response))

    # Set up.
    response = _recv(sock)
    result = _run_subprocess(command, response)
    state = result.pop('state')
    _send(sock, result)

    # Game play.
    while True:
        response = _recv(sock)
        if 'stop' in response:
            break
        response['state'] = state
        result = _run_subprocess(command, response)
        state = result.pop('state')
        _send(sock, result)

    # Scoring.
    print('%r' % response)


def _parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--name')
    parser.add_argument('--host')
    parser.add_argument('--port', type=int)
    parser.add_argument('--verbose', action='store_true')
    parser.add_argument('command', nargs='*')
    return parser.parse_args()


def main():
    args = _parse_args()

    # TODO: Set up logging.
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.WARNING)
    sock = _connect(args.host, args.port)
    _run(sock, args.name, args.command)


if __name__ == '__main__':
    main()
