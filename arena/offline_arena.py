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


def calculate_score_for_punter_mine(visited, punter_specific_adj, mine, distances, v):
    if visited.get(v) is not None:
        return 0

    visited[v] = True
    score = distances[mine][v] * distances[mine][v]
    for j in punter_specific_adj[v]:
        score += calculate_score_for_punter_mine(visited, punter_specific_adj, mine, distances, j)
    return score


def calculate_score_for_punter(punter, sites, mines, rivers, distances):
    score = 0
    for mine in mines:
        punter_specific_adj = []
        for site in sites:
            punter_specific_adj.append([])

        for river in rivers:
            if river[2] is not None and river[2] == punter:
                punter_specific_adj[river[0]].append(river[1])
                punter_specific_adj[river[1]].append(river[0])
        score += calculate_score_for_punter_mine({}, punter_specific_adj, mine, distances, mine)
    return score


def calculate_score(num_punters, sites, mines, rivers):
    adj = []
    for site in sites:
        l = []
        adj.append(l)

    for river in rivers:
        adj[river[0]].append(river[1])
        adj[river[1]].append(river[0])

    distances = {}
    for mine in mines:
        l = []
        for site in sites:
            l.append(0)
        distances[mine] = l

        visited = {}
        visited[mine] = True
        distances[mine][mine] = 0
        queue = [(mine, 0)]
        while len(queue) > 0:
            v, d = queue.pop()
            for j in adj[v]:
                if visited.get(j) is not None:
                    continue
                visited[j] = True
                distances[mine][j] = d + 1
                queue.append((j, d + 1))

    scores = []
    for punter in range(num_punters):
        scores.append(calculate_score_for_punter(punter, sites, mines, rivers, distances))
    return scores


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

        self._cap = None

        self._next_id = 0
        self._punters = {}

        self._map = Map(map_data)
        self._num_rivers = len(self._map.rivers)

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
        punter_id = self._generate_id()
        assert(punter_id not in self._punters)
        self._punters[punter_id] = {
            'player': player,
            'is_zombie': False,
            'state': None,
        }

        self._info('New player: %d' % punter_id)

        return punter_id

    def _get_current_client(self):
        return self._punters[self._turn % self._num_punters]

    def run(self):
        self._num_punters = len(self._punters)

        for i in range(self._num_punters):
            player = self._get_current_client()['player']
            player.prompt_setup(
                {'punter': self._turn % self._num_punters,
                 'punters': self._num_punters,
                 'map': map_data})

            self._turn += 1
        self._turn = 0
        while True:
            self._debug('Game #%d / %d' % (self._turn, self._num_rivers))
            if self._turn == self._num_rivers:
                self._info('Game over')

                moves = []
                while True:
                    try:
                        move = self._moves.popleft()
                        moves.append(move)
                    except IndexError:
                        break

                sys.stdout.write(json.dumps({'moves': moves}))
                sys.stdout.write('\n')
                sys.stdout.flush()

                result = 'Final rivers\n'
                rivers = self._map.rivers
                for river in rivers:
                    result += ('  %r\n' % (river,))
                self._debug(result)

                #rivers[1] = (rivers[1][0], rivers[1][1], 1)

                sys.stdout.write(json.dumps(calculate_score(
                    self._num_punters, self._map.sites, self._map.mines, self._map.rivers)))
                sys.stdout.write('\n')

                return

            moves = []
            start = len(self._moves) - min(len(self._moves), self._num_punters - 1)
            for i in range(start, len(self._moves)):
                moves.append(self._moves[i])

            client = self._get_current_client()
            client['player'].prompt_move(
                {'move': {'moves': moves},
                 'state': client['state']})
            self._turn += 1

    def ready(self, state):
        client = self._get_current_client()
        client['state'] = state

    def done_move(self, message, punter_id, is_move, source, target):
        new_state = message.get(u'state')
        del message[u'state']

        client = self._get_current_client()
        client[u'state'] = new_state
        if is_move:
            if source > target:
                tmp = source
                source = target
                target = tmp

            rivers = self._map.rivers
            for i in range(self._num_rivers):
                river = rivers[i]
                if river[0] == source and river[1] == target:
                    if river[2] is not None:
                        self._debug('Conflict')
                        self._moves.append({u'pass': {u'punter': new_state[u'punter']}})
                        return
                    else:
                        rivers[i] = (river[0], river[1], punter_id)
                        break
        self._moves.append(message)

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

            #self._debug('+Move')

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

                self._arena.done_move(message, punter_id, False, None, None)
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

                self._arena.done_move(message, punter_id, True, source, target)

            #self._debug('-Move')
        elif not self._received_handshake:
            #self._debug('+Handshake')

            self._received_handshake = True

            self._name = message.get(u'me')
            if self._name is None:
                self._handle_error('Bad handshake: %r' % message)
                return

            self._write({'you': self._name})

            #self._debug('-Hanshake')
        else:
            self._handle_error('Out of turn message: %r' % message)
            return


class FileEndpoint():
    def __init__(self, decode, logger):
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
                if len(self._buffer) == 0:
                    data = in_fd.read(5)
                else:
                    data = in_fd.read(1)
                if len(data) == 0:
                    self._debug('broken')
                    return
            else:
                data = in_fd.read(self._next_length + 1 + self._colon_pos - len(self._buffer))
            if not self._decode:
                data = data.decode('utf-8')
            prev_buffer_len = len(self._buffer)
            self._buffer += data
            if self._next_length is None:
                for i in range(len(data)):
                    if data[i] == ':':
                        self._colon_pos = i + prev_buffer_len
                        break
                if self._colon_pos is None:
                    continue
                if self._colon_pos == 0:
                    self._handle_error('Empty length from %r' % self._name)
                    return
                length_str = self._buffer[0:self._colon_pos]
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

        FileEndpoint.__init__(self, True, logger)
        PlayerHost.__init__(self)

        self._command = command

        self._name = None

        self._punter_id = arena.join(self)

    def _debug(self, s):
        self._logger.debug('S %s: %s' % (self._name, s))

    def _write(self, data):
        self._process.stdin.write(serialize(data))
        self._process.stdin.flush()

    def prompt_setup(self, setup):
        self._process = subprocess.Popen(self._command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        self._received_handshake = False
        self._read(self._process.stdout)

        self._setup_timeout_handle = True
        self._write(setup)
        self._read(self._process.stdout)

        self._process.kill()
        self._process = None

    def prompt_move(self, moves):
        self._process = subprocess.Popen(self._command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        self._received_handshake = False
        self._read(self._process.stdout)

        self._move_timeout_handle = True
        self._write(moves)
        self._read(self._process.stdout)

        self._process.kill()
        self._process = None


class Bot(FileEndpoint):
    def __init__(self, name, logger):
        FileEndpoint.__init__(self, False, logger)

        self._logger = logger

        self._name = name

        #self._debug('Created')

        self._done_handshake = False

        self._punter_id = None
        self._punters = None
        self._map = None

        self._write({'me': self._name})
        self._read(sys.stdin)

        self._read(sys.stdin)

    def _write(self, data):
        sys.stdout.write(serialize(data).decode('utf-8'))
        sys.stdout.flush()

    def _debug(self, s):
        self._logger.debug('C %s: %s' % (self._name, s))

    def _handle_message(self, message):
        if not self._done_handshake:
            #self._debug('+Handshake')
            #self._debug('-Handshake')
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
            #self._debug('%r received' % message)
            state = message.get(u'state')
            #self._write({'pass': {'punter': state['punter']}, 'state': state})
            self._write({'claim': {'punter': state[u'punter'], u'source': 0, u'target': 1}, 'state': state})
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
                    self._write(
                        {'claim': {'punter': self._punter_id,
                                   'source': river[0],
                                   'target': river[1]}})
                    self._debug('  Claim %d -> %d' % (river[0], river[1]))

                    #self._debug('-Move')
                    return
                i += 1


if __name__ == '__main__':
    parser = optparse.OptionParser()
    parser.add_option('--map', dest='map_file')
    parser.add_option('--commands', type='string', dest='commands', default=None)
    parser.add_option('--bot', default=None, dest='bot')
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
    else:
        map_file = open(options.map_file, 'r')
        map_data = json.loads(map_file.read())

        arena = Arena(map_data, logger)

        if options.commands is None:
            commands = [
                ['/usr/bin/python3', 'arena/offline_arena.py', '--bot', 'foo'],
                ['/usr/bin/python3', 'arena/offline_arena.py', '--bot', 'bar'],
                ['/usr/bin/python3', 'arena/offline_arena.py', '--bot', 'baz'],
                #['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'fooz'],
                # ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'bar'],
                # ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'baz'],
            ]
        else:
            commands = json.loads(options.commands)

        for command in commands:
            player = OfflinePlayerHost(command, arena, logger)

        arena.run()
