#!/usr/bin/python2

import json
import sys
import time
import optparse


def read_message():
    buf = ''
    while True:
        c = sys.stdin.read(1)
        if c == ':':
            break
        buf += c
    size = int(buf)
    text = sys.stdin.read(size)
    return json.loads(text)


def write_message(msg):
    text = json.dumps(msg)
    sys.stdout.write('%d:' % len(text))
    sys.stdout.write(text)
    sys.stdout.flush()


def on_setup(punter_id, num_punters, map):
    state = {
        'punter_id': punter_id,
        'num_punters': num_punters,
        'map': map,
    }
    return {'ready': punter_id, 'state': state}


def on_play(last_moves, state):
    time.sleep(10)
    return {'pass': {'punter': state['punter_id']}, 'state': state}


def on_stop(last_moves, scores, state):
    return {'ok': 'ok'}


def main():
    parser = optparse.OptionParser()
    parser.add_option('--bot', default=None, dest='bot')
    options, args = parser.parse_args(sys.argv[1:])

    while True:
        write_message({'me': options.bot})
        read_message()

        request = read_message()
        if 'punter' in request:
            response = on_setup(request['punter'], request['punters'], request['map'])
        elif 'move' in request:
            response = on_play(request['move']['moves'], request['state'])
        elif 'stop' in request:
            response = on_stop(request['stop']['moves'], request['stop']['scores'], request['state'])
        write_message(response)


if __name__ == '__main__':
    main()
