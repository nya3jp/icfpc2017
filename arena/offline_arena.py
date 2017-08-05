import asyncio
import collections
import functools
import json
import logging
import optparse
import socket
import socketserver
import subprocess
import sys
import threading


def serialize(o):
    data = json.dumps(o)
    return ('%d:%s' % (len(data), data)).encode('utf-8')


class Map():
    def __init__(self, map_data):
        self.sites = []
        for site in map_data[u'sites']:
            self.sites.append(site[u'id'])

        self.mines = map_data[u'mines']

        self.rivers = []
        for river in map_data[u'rivers']:
            source = river[u'source']
            target = river[u'target']
            if source > target:
                tmp = source
                source = target
                target = tmp
            self.rivers.append((source, target, None))


class Arena():
    def __init__(self, map_data, logger):
        self._logger = logger

        self._cap = 5

        self._next_id = 0
        self._clients = {}

        self._map = Map(map_data)

        self._turn = 0

        self._moves = collections.deque()

    def _debug(self, s):
        self._logger.debug('A: %s' % s)

    def _info(self, s):
        self._logger.info('A: %s' % s)

    def _generate_id(self):
        new_id = self._next_id
        self._next_id += 1
        return new_id

    def join(self, player):
        if len(self._clients) == self._cap:
            self._debug('Too many players')
            return None

        client_id = self._generate_id()
        assert(client_id not in self._clients)
        self._clients[client_id] = {
            'player': player,
            'is_zombie': False,
            'state': None,
        }

        self._info('New player: %d' % client_id)

        return client_id

    def run(self):
        for i in range(self._cap):
            self._debug('prompt_setup')
            player = self._get_current_client()['player']
            player.prompt_setup_soon({'punter': self._turn % self._cap, 'punters': self._cap, 'map': map_data})

            self._turn += 1
        while True:
            if self._turn == self._cap + len(self._map.rivers):
                self._info('Game over')

                rivers = self._map.rivers
                for river in rivers:
                    self._logger.info(river)

                return

            moves = []
            if len(self._moves) == self._cap:
                start = 1
            else:
                start = 0
            for i in range(start, len(self._moves)):
                moves.append(self._moves[i])

            client = self._get_current_client()
            client['player'].prompt_move_soon({'move': {'moves': moves}, 'state': client['state']})
            self._turn += 1

    def ready(self, state):
        client = self._get_current_client()
        client['state'] = state

    def done_move(self, message, punter_id, is_move, source, target, new_state):
        client = self._get_current_client()
        client['state'] = new_state
        if is_move:
            rivers = self._map.rivers
            for i in range(len(rivers)):
                river = rivers[i]
                if river[0] == source and river[1] == target:
                    if river[2] is not None:
                        self._logger.error('Conflict')
                        pass
                    rivers[i] = (river[0], river[1], punter_id)
            self._moves.append(message)
            if len(self._moves) > self._cap:
                self._moves.popleft()

    def _get_current_client(self):
        return self._clients[self._turn % self._cap]

    def handle_error(self, s):
        self._debug(s)


class PlayerHost():
    def __init__(self):
        self._setup_timeout_handle = None
        self._move_timeout_handle = None

    def _timeout(self):
        if self._setup_timeout_handle is not None:
            self._arena.setup_timeout()
        elif self._move_timeout_handle is not None:
            self._arena.move_timeout()

    def _handle_error(self, s):
        self._arena.handle_error(s)

    def _handle_message(self, message):
        if self._setup_timeout_handle is not None:
            if self._setup_timeout_handle is not True:
                self._setup_timeout_handle.cancel()
            self._setup_timeout_handle = None

            self._debug('+Setup')
            ready = message.get(u'ready')
            if ready is None or ready != self._punter_id:
                self._handle_error('Bad ready: %r' % message)
                return
            state = message.get(u'state')
            self._arena.ready(state)

            self._debug('-Setup')
        elif self._move_timeout_handle is not None:
            if self._move_timeout_handle is not True:
                self._move_timeout_handle.cancel()
            self._move_timeout_handle = None

            self._debug('+Move')
            new_state = message.get(u'state')
            claim = message.get(u'claim')
            if claim is None:
                pass_desc = message.get(u'pass')
                if pass_desc is None:
                    self._handle_error('Bad move: %r' % message)
                    return
                punter_id = pass_desc.get(u'punter')
                if punter_id is None or punter_id != self._punter_id:
                    self._handle_error('Bad move (wrong id): %r' % message)
                    return

                self._debug('  Pass')

                self._arena.done_move(message, punter_id, False, None, None, new_state)
            else:
                punter_id = claim.get(u'punter')
                if punter_id is None or punter_id != self._punter_id:
                    self._handle_error('Bad move: %r' % message)
                    return
                source = claim.get(u'source')
                if source is None:
                    self._handle_error('Bad move: %r' % message)
                    return
                target = claim.get(u'target')
                if target is None:
                    self._handle_error('Bad move: %r' % message)
                    return

                self._debug('  Claim %d -> %d' % (source, target))

                self._arena.done_move(message, punter_id, True, source, target, new_state)

            self._debug('-Move')
            self._debug('+Move')
        elif not self._received_handshake:
            self._debug('hoge')
            self._received_handshake = True

            self._name = message.get(u'me')
            if self._name is None:
                self._handle_error('Bad handshake: %r' % message)
                return

            self._write({'you': self._name})

            self._debug('-Hanshake')
        else:
            self._handle_error('Out of turn message: %r' % message)
            return


class SocketPlayerHost(PlayerHost):
    def __init__(self, socket):
        self._buffer = b''
        self._colon_pos = -1
        self._next_length = None

        self._socket = socket
        self._socket.setblocking(False)

        self._loop = asyncio.new_event_loop()
        self._loop.add_reader(self._socket, functools.partial(self._recv))

    def start(self):
        self._loop.run_forever()

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

    def _send(self, data):
        self._socket.send(serialize(data))

    def _recv(self):
        while True:
            try:
                data = self._socket.recv(4096)
            except BlockingIOError:
                return
            if not data:
                return
            self._buffer += data

            if self._next_length is None:
                self._colon_pos = self._buffer.find(b':')
                if self._colon_pos == -1:
                    continue
                if self._colon_pos == 0:
                    self._handle_error('Empty length from %r' % self._name)
                    return

                length_str = self._buffer[0:self._colon_pos].decode('utf-8')
                self._next_length = int(length_str)
                if str(self._next_length) != length_str:
                    self._handle_error('Bad length from %r: %s' % (self._name, length_str))
                    return
            if len(self._buffer) < self._colon_pos + 1 + self._next_length:
                continue

            message_start = self._colon_pos + 1
            message_end = self._colon_pos + 1 + self._next_length
            try:
                message_str = self._buffer[message_start:message_end]
                message = json.loads(message_str.decode('utf-8'))
            except json.JSONDecodeError as e:
                self._handle_error('Bad message: %s' % message_str)
                return

            self._buffer = self._buffer[message_end:]
            self._colon_pos = -1
            self._next_length = None

            self._handle_message(message)

    def _handle_error(self, detail):
        pass


class FileEndpoint():
    def __init__(self, logger, decode):
        self._logger = logger
        self._decode = not decode
        if decode:
            self._buffer = ''
        else:
            self._buffer = ''
        self._colon_pos = None
        self._next_length = None

    def _debug(self, s):
        self._logger.debug(s)

    def _read(self, in_fd):
        while True:
            if self._next_length is None:
                data = in_fd.read(1)
                if len(data) == 0:
                    self._debug('broken')
                    return
            else:
                data = in_fd.read(self._next_length + self._colon_pos - len(self._buffer))
            if not self._decode:
                data = data.decode('utf-8')
            self._buffer += data
            if self._next_length is None:
                for i in range(len(data)):
                    if data[i] == ':':
                        self._colon_pos = i + len(self._buffer)
                        break
                if self._colon_pos is None:
                    continue
                if self._colon_pos == 0:
                    self._handle_error('Empty length from %r' % self._name)
                    return
                length_str = self._buffer[0:self._colon_pos - 1]
                self._next_length = int(length_str)
                if str(self._next_length) != length_str:
                    self._handle_error('Bad length from %r: %s' % (self._name, length_str))
                    return

            if len(self._buffer) < self._colon_pos + self._next_length:
                continue

            message_start = self._colon_pos
            message_end = self._colon_pos + self._next_length
            try:
                message_str = self._buffer[message_start:message_end]
                message = json.loads(message_str)
            except json.JSONDecodeError as e:
                self._handle_error('Bad message: %s' % message_str)
                return

            self._buffer = self._buffer[message_end:]
            self._colon_pos = None
            self._next_length = None

            self._handle_message(message)
            return


class OfflinePlayerHost(PlayerHost, FileEndpoint):
    def __init__(self, command, arena, logger):
        self._logger = logger
        self._arena = arena

        FileEndpoint.__init__(self, logger, True)
        PlayerHost.__init__(self)

        self._command = command

        self._name = None

        self._punter_id = arena.join(self)

    def _debug(self, s):
        self._logger.debug('S %s: %s' % (self._name, s))

    def _write(self, data):
        self._process.stdin.write(serialize(data))
        self._process.stdin.flush()

    def prompt_setup_soon(self, setup):
        self._debug(self._command)
        self._received_handshake = False
        self._process = subprocess.Popen(self._command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self._read(self._process.stdout)
        self._setup_timeout_handle = True
        self._write(setup)
        self._read(self._process.stdout)
        self._process.terminate()
        self._process = None

    def prompt_move_soon(self, moves):
        self._debug(self._command)
        self._received_handshake = False
        self._process = subprocess.Popen(self._command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self._read(self._process.stdout)
        self._move_timeout_handle = True
        self._write(moves)
        self._read(self._process.stdout)
        self._process.terminate()
        self._process = None


class RequestHandler(socketserver.BaseRequestHandler, SocketPlayerHost):
    def handle(self):
        self._arena = self.server.arena
        self._logger = self.server.logger

        self._received_handshake = False

        self._name = None
        self._punter_id = None

        self._setup_timeout_handle = None
        self._move_timeout_handle = None

        SocketPlayerHost.__init__(self, self.request)

        self._debug('+Hanshake')

        self.start()

    def stop(self):
        self._loop.stop()

    def _debug(self, s):
        self._logger.debug('S %s: %s' % (self._name, s))


class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    def __init__(self, arena, options, logger):
        self.logger = logger

        self.arena = arena

        socketserver.TCPServer.__init__(self, (options.server_host, options.port), RequestHandler)


class Bot(FileEndpoint):
    def __init__(self, name, logger):
        self._logger = logger

        FileEndpoint.__init__(self, logger, False)
        self._name = name

        self._debug('Created')

        self._done_handshake = False

        self._punter_id = None
        self._punters = None
        self._map = None

        self._write({'me': self._name})
        sys.stdout.flush()
        self._read(sys.stdin)

        self._read(sys.stdin)
        self._read(sys.stdin)
        self._read(sys.stdin)
        self._read(sys.stdin)
        self._read(sys.stdin)
        self._read(sys.stdin)

    def _write(self, data):
        sys.stdout.write(serialize(data).decode('utf-8'))
        sys.stdout.flush()

    def _debug(self, s):
        self._logger.debug('C %s: %s' % (self._name, s))

    def _handle_message(self, message):
        if not self._done_handshake:
            self._debug('+Handshake')
            self._debug('-Handshake')
            self._done_handshake = True
        elif message.get(u'punter') is not None:
            self._debug('+Setup')

            self._punter_id = message[u'punter']
            self._punters = message[u'punters']
            self._map = Map(message[u'map'])

            self._debug('  punter: %d, punters: %d, num_sites: %d, num_mines: %d, num_rivers: %d' %
                        (self._punter_id, self._punters, len(self._map.sites), len(self._map.mines), len(self._map.rivers)))
            self._write({'ready': self._punter_id, 'state': {'punter': self._punter_id}})

            self._debug('-Setup')
        else:
            self._debug('+Move')
            self._debug('%r received' % message)
            state = message.get(u'state')
            self._write({'pass': {'punter': state['punter']}, 'state': state})
            return
            rivers = self._map.rivers
            for move in message[u'move'][u'moves']:
                claim = move.get(u'claim')
                if claim is not None:
                    punter_id = claim[u'punter']
                    source = claim[u'source']
                    target = claim[u'target']
                    if source > target:
                        tmp = target
                        target = source
                        source = tmp
                    i = 0
                    while i < len(rivers):
                        river = rivers[i]
                        if river[0] == source and river[1] == target:
                            rivers[i] = (source, target, punter_id)
                        i += 1
            i = 0
            while i < len(rivers):
                river = rivers[i]
                if river[2] is None:
                    rivers[i] = (river[0], river[1], self._punter_id)
                    self._write({'claim': {'punter': self._punter_id, 'source': river[0], 'target': river[1]}})
                    self._debug('  Claim %d -> %d' % (river[0], river[1]))

                    self._debug('-Move')
                    return
                i += 1


if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.add_option('--server_host', dest='server_host')
    parser.add_option('--port', type='int', dest='port')
    parser.add_option('--map', dest='map_file')
    parser.add_option('--commands', type='string', dest='commands', default=None)
    parser.add_option('--bot', default=None, dest='bot')
    parser.add_option('--fake_clients', type='int', default='-1', dest='fake_clients')
    parser.add_option('--log-level', '--log_level', type='choice',
                      dest='log_level', default='debug',
                      choices=['debug', 'info', 'warning', 'error',
                               'critical'],
                      help='Log level.')
    options, args = parser.parse_args(sys.argv[1:])

    logging.basicConfig()
    logger = logging.getLogger()
    logger.setLevel(logging.getLevelName(options.log_level.upper()))

    if options.bot is not None:
        Bot(options.bot, logger)

    map_file = open(options.map_file, 'r')
    map_data = json.loads(map_file.read())

    arena = Arena(map_data, logger)

    options.commands = json.dumps([
        ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'a'],
        ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'b'],
        ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'c'],
        ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'd'],
        ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'e'],
    ])
    if options.commands is not None:
        commands = json.loads(options.commands)
        for command in commands:
            player = OfflinePlayerHost(command, arena, logger)
        arena.run()

    if options.fake_clients != -1:
        for i in range(options.fake_clients):
            client = Client('%d' % i, options, logger)
            client_thread = threading.Thread(target=client.run)
            client_thread.daemon = True
            client_thread.start()

    server_thread.join()
