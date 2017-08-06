import collections
import json
import logging
import optparse
import subprocess
import sys
import time


def serialize(o):
    data = json.dumps(o)
    return ('%d:%s' % (len(data), data)).encode('utf-8')


class Map():
    def __init__(self, map_data, logger):
        self._logger = logger

        self.sites = []
        for site in map_data['sites']:
            self.sites.append(site['id'])

        self.mines = map_data['mines']

        adj = []
        for site in self.sites:
            l = []
            adj.append(l)

        self.rivers = []
        for river in map_data['rivers']:
            source = river['source']
            target = river['target']

            adj[source].append(target)
            adj[target].append(source)

            if source > target:
                tmp = source
                source = target
                target = tmp
            self.rivers.append((source, target, None))

        self.distances = {}
        for mine in self.mines:
            l = []
            for site in self.sites:
                l.append(float('inf'))
            self.distances[mine] = l

            visited = {}
            visited[mine] = True
            self.distances[mine][mine] = 0
            queue = collections.deque([(mine, 0)])
            while len(queue) > 0:
                v, d = queue.popleft()
                for j in adj[v]:
                    if visited.get(j) is not None:
                        continue
                    visited[j] = True
                    self.distances[mine][j] = d + 1
                    queue.append((j, d + 1))

        self.scores = []
        self._futures = {}

    def _debug(self, s):
        self._logger.debug('Map: %s' % s)

    def set_num_punters(self, num_punters):
        self._num_punters = num_punters
        for i in range(num_punters):
            self.scores.append((0, 0))

    def set_futures(self, futures, punter_id):
        self._futures[punter_id] = futures

    def calculate_score(self, done):
        for i in range(self._num_punters):
            self.scores[i] = self._calculate_score_for_punter(i, self._futures.get(i), done)

        log = []
        for x, y in self.scores:
            if done:
                log.append('%d' % x)
            else:
                log.append('%d+-%d' % (x, y))
        self._debug('scores: %s' % ' '.join(log))

    def _calculate_score_for_punter(self, punter, futures, done):
        punter_specific_adj = []
        for site in self.sites:
            punter_specific_adj.append([])

        for river in self.rivers:
            if river[2] is not None and river[2] == punter:
                punter_specific_adj[river[0]].append(river[1])
                punter_specific_adj[river[1]].append(river[0])

        score = 0
        potential_change = 0
        for mine in self.mines:
            visited = {}
            score += self._calculate_score_for_punter_mine(visited, punter_specific_adj, mine, mine)
            if futures is None:
                continue
            for f in futures:
                if f['source'] == mine:
                    futures_target = f['target']
                    futures_score = self.distances[mine][futures_target] ** 3
                    self._debug('futures (punter %d, mine %d, target %d): %d' % (punter, mine, futures_target, futures_score))
                    if visited.get(futures_target) is None:
                        if done:
                            score -= futures_score
                        else:
                            potential_change += futures_score
                    else:
                        score += futures_score
        return score, potential_change

    def _calculate_score_for_punter_mine(self, visited, punter_specific_adj, mine, v):
        if visited.get(v) is not None:
            return 0

        visited[v] = True
        base_score = self.distances[mine][v]
        score = base_score ** 2
        for j in punter_specific_adj[v]:
            score += self._calculate_score_for_punter_mine(visited, punter_specific_adj, mine, j)
        return score


class Arena():
    def __init__(self, map_data, options, logger):
        self._logger = logger

        self._options = options

        self._next_id = 0

        self._punters = {}

        self._map = Map(map_data, logger)
        self._num_rivers = len(self._map.rivers)

    def _debug(self, s):
        self._logger.debug('Arena: %s' % s)

    def _info(self, s):
        self._logger.info('Arena: %s' % s)

    def join(self, punter):
        punter_id = self._next_id
        self._next_id += 1

        self._punters[punter_id] = punter

        return punter_id

    def _get_current_punter(self):
        return self._punters[self._step % self._num_punters]

    def run(self):
        self._num_punters = len(self._punters)
        self._map.set_num_punters(self._num_punters)

        self._all_moves = []

        for i in range(self._num_punters):
            punter = self._punters[i]
            punter.prompt_setup(
                {'punter': i,
                 'punters': self._num_punters,
                 'map': map_data,
                 'settings': {'futures': True, 'gennai-persistent': True}})

        self._step = 0

        self._moves = collections.deque()
        for i in range(self._num_punters):
            self._moves.append({'pass': {'punter': i}})

        while True:
            self._debug('step #%d / %d' % (self._step, self._num_rivers))

            if self._step == self._num_rivers:
                self._info('Game over')

                result = 'Final rivers\n'
                rivers = self._map.rivers
                for river in rivers:
                    result += ('  %r\n' % (river,))
                self._debug(result)

                self._map.calculate_score(True)

                sys.stdout.write(
                    json.dumps(
                        {'moves': self._all_moves,
                         'scores': [x for x, y in self._map.scores]}))
                sys.stdout.write('\n')
                sys.stdout.flush()

                return

            punter = self._get_current_punter()
            punter.prompt_move({'move': {'moves': list(self._moves)}})

            if self._options.log_score_every_step:
                self._map.calculate_score(False)

            self._step += 1

    def done_setup(self, futures, punter_id):
        self._map.set_futures([{'source': 1, 'target': 6}], 1)
        #self._map.set_futures(futures, punter_id)

    def done_move(self, message, punter_id, is_move, source, target, time_spent_ms):
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
                        self._debug('Conflict: %r' % river[2])

                        if options.include_cause:
                            full = {'pass': {'punter': punter_id}, 'cause': message}
                        else:
                            full = {'pass': {'punter': punter_id}}
                        stripped = {'pass': {'punter': punter_id}}
                    else:
                        rivers[i] = (river[0], river[1], punter_id)

                        full = message
                        stripped = {'claim': message['claim']}
                    break
        else:
            full = message
            stripped = {'pass': message['pass']}

        self._moves.append(stripped)
        self._moves.popleft()
        if options.include_state:
            self._all_moves.append(full)
        else:
            self._all_moves.append(stripped)

        if options.include_time:
            self._all_moves[-1] = dict(self._all_moves[-1])
            self._all_moves[-1]['time'] = time_spent_ms


class FileEndpoint():
    def __init__(self):
        self._message_length = None
        self._buffer = ''

    def _read(self, in_file):
        while True:
            if self._message_length is None:
                data = in_file.read(1).decode('utf-8')
                if len(data) == 0:
                    self._debug('EOF')
                    return

                if data != ':':
                    self._buffer += data
                    continue

                if len(self._buffer) == 0:
                    self._debug('Empty length')
                    return

                length_str = self._buffer
                self._message_length = int(length_str)
                if str(self._message_length) != length_str:
                    self._debug('Bad length: %s' % length_str)
                    return
                self._buffer = ''

            data = in_file.read(self._message_length - len(self._buffer)).decode('utf-8')
            if len(data) == 0:
                self._debug('EOF')
                return

            self._buffer += data

            if len(self._buffer) < self._message_length:
                continue

            try:
                message = json.loads(self._buffer)
            except json.JSONDecodeError as e:
                self._handle_error('Bad message: %s' % message_str)
                return

            self._message_length = None
            self._buffer = ''

            self._handle_message(message)
            return


class OfflinePunterHost(FileEndpoint):
    def __init__(self, command, arena, options, logger):
        self._logger = logger

        self._options = options

        self._arena = arena

        self._command = command
        self._process = None

        FileEndpoint.__init__(self)

        self._handling_handshake = False

        self._handling_setup = False
        self._handling_move = False

        self._name = None

        self._punter_id = arena.join(self)

        self._debug('%d: Constructed' % self._punter_id)

    def _debug(self, s):
        self._logger.debug('%s: %s' % (self._name, s))

    def _write(self, data):
        self._process.stdin.write(serialize(data))
        self._process.stdin.flush()

    def _handle_message(self, message):
        if self._handling_handshake:
            self._handling_handshake = False

            self._name = message.get('me')
            if self._name is None:
                self._handle_error('Bad handshake: %r' % message)
                return

            self._write({'you': self._name})
        elif self._handling_setup:
            self._handling_setup = False

            ready = message.get('ready')
            if ready is None or ready != self._punter_id:
                self._handle_error('Bad ready: %r' % message)
                return

            self._game_state = message.get('state')

            if self._options.feature_negotiation and message.get('gennai-persistent') is None:
                self._options.persistent = False

            self._arena.done_setup(message.get('futures'), self._punter_id)
        elif self._handling_move:
            self._handling_move = False

            time_spent_ms = (time.perf_counter() - self._start_time) * 1000
            self._debug('move: %d ms' % time_spent_ms)

            claim = message.get('claim')
            if claim is None:
                pass_desc = message.get('pass')
                if pass_desc is None:
                    self._handle_error('Bad message: %r' % message)
                    return

                punter_id = pass_desc.get('punter')
                if punter_id is None or punter_id != self._punter_id:
                    self._handle_error(
                        'Bad move (punter id missing or wrong): %r' % message)
                    return

                self._debug('pass')

                self._game_state = message.get('state')

                self._arena.done_move(message, punter_id, False, None, None, time_spent_ms)
            else:
                punter_id = claim.get('punter')
                if punter_id is None or punter_id != self._punter_id:
                    self._handle_error('Bad move (punter id missing or wrong): %r' % message)
                    return
                source = claim.get('source')
                if source is None:
                    self._handle_error('Bad move (source missing): %r' % message)
                    return
                target = claim.get('target')
                if target is None:
                    self._handle_error('Bad move (target missing): %r' % message)
                    return

                self._debug('claim %d -> %d' % (source, target))

                self._arena.done_move(message, punter_id, True, source, target, time_spent_ms)

                self._game_state = message.get('state')
        else:
            self._debug('Unsolicited message: %r' % message)

    def _launch(self):
        if self._process is None:
            if self._options.persistent:
                self._command += ['--persistent']
            self._process = subprocess.Popen(self._command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self._handling_handshake = True
        self._read(self._process.stdout)

    def _kill(self):
        if not self._options.persistent:
            self._debug('killing')
            self._process.kill()
            self._process = None

    def prompt_setup(self, setup):
        self._launch()

        self._handling_setup = True
        self._start_time = time.perf_counter()
        self._write(setup)
        self._read(self._process.stdout)
        time_spent_ms = (time.perf_counter() - self._start_time) * 1000
        self._debug('setup: %d ms' % time_spent_ms)

        self._kill()

    def prompt_move(self, moves):
        self._launch()

        self._handling_move= True
        if self._game_state is not None:
            moves['state'] = self._game_state
        self._start_time = time.perf_counter()
        self._write(moves)
        self._read(self._process.stdout)

        self._kill()


if __name__ == '__main__':
    parser = optparse.OptionParser()

    parser.add_option('--map', dest='map_file')
    parser.add_option('--commands', type='string', dest='commands', default=None)

    # Include state in the move history to be output.
    parser.add_option('--include_state', action='store_true', dest='include_state')
    # Include time spent for each move in the move history.
    parser.add_option('--include_time', action='store_true', dest='include_time')
    # Include the original move in the move history when conflict happens.
    parser.add_option('--include_cause', action='store_true', dest='include_cause')

    parser.add_option('--feature_negotiation', action='store_true', dest='feature_negotiation')
    parser.add_option('--persistent', action='store_true', dest='persistent')

    parser.add_option('--log-level', '--log_level', type='choice',
                      dest='log_level', default='debug',
                      choices=['debug', 'info', 'warning', 'error',
                               'critical'],
                      help='Log level.')

    parser.add_option('--log_score_every_step', action='store_true', dest='log_score_every_step')

    options, args = parser.parse_args(sys.argv[1:])

    logging.basicConfig()
    logger = logging.getLogger()
    logger.setLevel(logging.getLevelName(options.log_level.upper()))

    map_file = open(options.map_file, 'r')
    map_data = json.loads(map_file.read())

    arena = Arena(map_data, options, logger)

    if options.commands is None:
        commands = [
            #['framework/game_main', '--name', 'fooz', '--punter', 'GreedyPunter'],
             ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'bar'],
             ['/usr/bin/python3', 'punter/pass-py/pass.py', '--bot', 'baz'],
        ]
    else:
        commands = json.loads(options.commands)

    for command in commands:
        punter = OfflinePunterHost(command, arena, options, logger)

    arena.run()
