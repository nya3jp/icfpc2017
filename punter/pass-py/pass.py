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


def on_setup(punter_id, num_punters, map, options):
    delay = options.sleep_ms_in_setup
    if delay is not None:
        time.sleep(delay / 1000.0)

    state = {
        'punter_id': punter_id,
        'num_punters': num_punters,
        'map': map,
    }
    return {'ready': punter_id, 'state': state}


def on_play(last_moves, state, options):
    delay = options.sleep_ms_in_play
    if delay is not None:
        time.sleep(delay / 1000.0)

    return {'pass': {'punter': state['punter_id']}, 'state': state}


def on_stop(last_moves, scores, state):
    return {'ok': 'ok'}


def main():
    parser = optparse.OptionParser()
    parser.add_option('--bot', dest='bot', default=None)
    parser.add_option('--persistent', dest='persistent', default=None, action='store_true')
    parser.add_option('--sleep_ms_in_setup', dest='sleep_ms_in_setup', type='int', default=None)
    parser.add_option('--sleep_ms_in_play', dest='sleep_ms_in_play', type='int', default=None)
    options, args = parser.parse_args(sys.argv[1:])

    while True:
        write_message({'me': options.bot})
        read_message()

        request = read_message()
        if 'punter' in request:
            response = on_setup(request['punter'], request['punters'], request['map'], options)
        elif 'move' in request:
            response = on_play(request['move']['moves'], request['state'], options)
        elif 'stop' in request:
            response = on_stop(request['stop']['moves'], request['stop']['scores'], request['state'])
        write_message(response)


if __name__ == '__main__':
    main()
