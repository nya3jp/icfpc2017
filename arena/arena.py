import asyncio
import functools
import json
import logging
import optparse
import socketserver
import sys
import threading


def serialize(o):
    data = json.dumps(o)
    return '%d:%s' % (len(data), data)


class Arena(object):
    def __init__(self, map_data, logger):
        self._logger = logger

        self._cap = 2

        self._sites = map_data[u'sites']
        self._mines = map_data[u'mines']
        self._rivers = map_data[u'rivers']

        # for site in self._sites:
        #     print 'Site %d (%f, %f)' % (site[u'id'], site[u'x'], site[u'y'])
        # for mine in self._mines:
        #     print '%d' % mine
        # for river in self._rivers:
        #     print '%d -> %d' % (river[u'source'], river[u'target'])

        self._next_id = 0
        self._clients = {}

        self._setup = None

        self._turn = 0

        self._lock = threading.Lock()


    def _generate_id(self):
        new_id = self._next_id
        self._next_id += 1
        return new_id

    def _next_client(self):
        self._turn = (self._turn + 1) % self._cap

    def join(self, handler, name):
        self._lock.acquire()

        self._logger.info('Join: %s' % name)

        if len(self._clients) == self._cap:
            return None

        client_id = self._generate_id()
        assert(client_id not in self._clients)
        self._clients[client_id] = {
            'name': name,
            'handler': handler,
            'zombie': False
        }

        self._logger.info('Join: %d' % client_id)

        if len(self._clients) == self._cap:
            self.done_setup()

        self._lock.release()

    def done_setup(self):
        client = self._clients[self._turn]
        self._logger.info('done_setup() Name: %s' % client['name'])
        client['handler'].prompt_setup_soon({"punter": self._turn, "punters": self._cap, "map": map_data})
        self._turn = self._turn + 1
        if self._turn == self._cap:
            self._turn = 0
            self._done_move(None, None, None)

    def done_move(self, message, source, target):
        if message is not None:
            self._moves.append(message)
        self._clients[self._turn].prompt_move_soon({"move": {"moves": list(self._moves)}})
        self._next_client()

    def done_move_pass(self, message):
        self._moves.append(message)
        self._clients[self._turn].prompt_move_soon({"move": {"moves": list(self._moves)}})
        self._next_client()


class RequestHandler(socketserver.BaseRequestHandler):
    def handle(self):
        self._arena = self.server.arena
        self._logger = self.server.logger

        self._received_handshake = False

        self._name = None
        self._punter_id = None

        self._buffer = b''
        self._colon_pos = -1
        self._next_length = None

        self._setup_timeout_handle = None
        self._move_timeout_handle = None

        self.request.setblocking(False)

        self._loop = asyncio.new_event_loop()
        self._loop.add_reader(self.request, functools.partial(self._recv))
        self._loop.run_forever()
        self._loop.close()

    def _send(self, data):
        self.request.send(serialize(data).encode('utf-8'))

    def prompt_setup_soon(self, setup):
        self._loop.call_soon_threadsafe(functools.partial(self._prompt_setup, setup))

    def _prompt_setup(self, setup):
        self._send(setup)
        self._setup_timeout_handle = self._loop.call_later(10, functools.partial(self._timeout))

    def prompt_move_soon(self, moves):
        self._loop.call_soon_threadsafe(functools.partial(self._prompt_move, moves))

    def _prompt_move(self, moves):
        self._send(moves)
        self._move_timeout_handle = self._loop.call_later(1, functools.partial(self._timeout))

    def _timeout(self):
        self._arena.done_move_pass({"pass": {"punter": self._punter_id}})

    def _handle_error(self, details):
        self._logger.info(details)
        self._arena.set_zombie()
        self._loop.stop()

    def _recv(self):
        while True:
            try:
                data = self.request.recv(4096)
            except BlockingIOError:
                return
            if not data:
                return
            self._buffer += data

            if self._next_length is None:
                if self._colon_pos == -1:
                    self._colon_pos = self._buffer.find(b':')
                if self._colon_pos == -1:
                    continue
                if self._colon_pos == 0:
                    self._handle_error('Empty length from %r' % self._name)
                    return

                self._logger.info('_colon_pos: %d' % self._colon_pos)

                length_str = self._buffer[0:self._colon_pos].decode('utf-8')
                self._next_length = int(length_str)
                if str(self._next_length) != length_str:
                    self._handle_error('Bad length from %r: %s' % (self._name, length_str))
                    return
                self._logger.info('_next_length: %d' % self._next_length)
            if len(self._buffer) < self._colon_pos + 1 + self._next_length:
                continue

            message_start = self._colon_pos + 1
            message_end = self._colon_pos + 1 + self._next_length
            try:
                message_str = self._buffer[message_start:message_end]
                self._logger.info('message_str: %r' % message_str)
                message = json.loads(message_str.decode('utf-8'))
            except json.JSONDecodeError as e:
                self._handle_error('Bad message from %r: %s' % (self._name, message_str))
                return

            self._buffer = self._buffer[message_end:]
            self._colon_pos = -1
            self._next_length = None

            if self._setup_timeout_handle is not None:
                self._setup_timeout_handle.cancel()
                self._arena.done_setup()
                ready = message.get(u'ready')
                if ready is None or ready != self._punter_id:
                    self._handle_error('Bad ready from %s: %r' % (self._name, message))
                    return
            elif self._move_timeout_handle is not None:
                self._move_timeout_handle.cancel()
                claim = message.get(u'claim')
                if claim is None:
                    pass_desc = message.get(u'pass')
                    if pass_desc is None:
                        self._handle_error('Bad move from %s: %r' % (self._name, message))
                        return
                    punter_id = pass_desc.get(u'punter')
                    if punter_id is None or punter_id != self._punter_id:
                        self._handle_error('Bad move from %s: %r' % (self._name, message))
                        return
                    self._arena.done_move_pass(message)
                else:
                    punter_id = claim.get(u'punter')
                    if punter_id is None or punter_id != self._punter_id:
                        self._handle_error('Bad move from %s: %r' % (self._name, message))
                        return
                    source = claim.get(u'source')
                    if source is None:
                        self._handle_error('Bad move from %s: %r' % (self._name, message))
                        return
                    target = claim.get(u'target')
                    if target is None:
                        self._handle_error('Bad move from %s: %r' % (self._name, message))
                        return
                    self._arena.done_move(self, source, target)
            elif not self._received_handshake:
                self._received_handshake = True
                self._name = message.get(u'me')
                if self._name is None:
                    self._handle_error('Bad handshake: %r' % message)
                    return
                self._send({"you": self._name})
                self._punter_id = self._arena.join(self, self._name)
            else:
                self._handle_error('Out of turn message from %r: %r' % (self._name, message))
                return


class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    def __init__(self, options, arena, logger):
        self.arena = arena
        self.logger = logger

        socketserver.TCPServer.__init__(self, (options.server_host, options.port), RequestHandler)


if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.add_option('--server_host', dest='server_host')
    parser.add_option('--port', type='int',dest='port')
    parser.add_option('--map', dest='map_file')
    parser.add_option('--log-level', '--log_level', type='choice',
                      dest='log_level', default='debug',
                      choices=['debug', 'info', 'warning', 'error',
                               'critical'],
                      help='Log level.')
    options, args = parser.parse_args(sys.argv[1:])

    logging.basicConfig()
    logger = logging.getLogger()
    logger.setLevel(logging.getLevelName(options.log_level.upper()))
    logger.info('hoge')

    map_file = open(options.map_file, 'r')
    map_data = json.loads(map_file.read())

    arena = Arena(map_data, logger)

    server = Server(options, arena, logger)
    server.serve_forever()
