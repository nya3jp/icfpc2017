#!/usr/bin/python2

import binascii
import hashlib
import itertools
import logging
import os
import random
import shlex
import subprocess
import sys
import tempfile
import time

import gflags
import pymongo
import ujson
import yaml

FLAGS = gflags.FLAGS

gflags.DEFINE_bool('stadium', False, 'Use stadium instead of arena')
gflags.DEFINE_bool('auto_restart', False, 'Enable auto restart')

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
DATA_DIR = os.path.join(BASE_DIR, 'lambdapark/data')

g_last_check = 0


def setup_logging():
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s.%(msecs)03d %(levelname)s [%(filename)s:%(lineno)d] %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S')


def build_requirements():
    args = [
        'bazel',
        'build',
        '-c',
        'opt',
        '//stadium:stadium',
    ]
    subprocess.check_call(args, cwd=BASE_DIR)


def restart():
    os.execv(sys.executable, [sys.executable] + sys.argv)


def check_update():
    global g_last_check
    if not FLAGS.auto_restart:
        return
    now = time.time()
    if now - g_last_check < 180:
        return
    g_last_check = now
    last_rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'])
    subprocess.call(['git', 'pull', '--rebase'])
    next_rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'])
    if next_rev != last_rev:
        logging.info('Detected an update. Restarting...')
        restart()


def load_settings():
    with open(os.path.join(BASE_DIR, 'lambdapark/settings.yaml')) as f:
        return yaml.load(f)


def load_result_index(config):
    path = os.path.join(DATA_DIR, config['name'], 'index.json')
    if not os.path.exists(path):
        return {'results': []}
    with open(path) as f:
        return ujson.load(f)


def save_result(result, config, punters, output_prefix, start_time, end_time):
    punter_names = [p['name'] for p in punters]
    result_path = output_prefix + '.json'
    fat_result = result.copy()
    fat_result.update({
        'punters': punter_names,
        'start_time': start_time,
        'duration': end_time - start_time,
    })
    thin_result = {
        'result_file': os.path.basename(result_path),
        'log_file': os.path.basename(output_prefix) + '.log',
        'punters': punter_names,
        'scores': fat_result['scores'],
        'start_time': start_time,
        'duration': end_time - start_time,
    }

    with open(result_path, 'w') as f:
        ujson.dump(fat_result, f)

    index = load_result_index(config)
    index['results'].append(thin_result)
    index_path = os.path.join(DATA_DIR, config['name'], 'index.json')
    with open(index_path, 'w') as f:
        ujson.dump(index, f)


def run_common(args, output_prefix):
    with tempfile.TemporaryFile() as stdout:
        with open(os.path.join(output_prefix + '.log'), 'w') as stderr:
            with open(os.devnull) as null:
                proc = subprocess.Popen(
                    args,
                    cwd=BASE_DIR,
                    stdin=null,
                    stdout=stdout,
                    stderr=stderr)
        proc.wait()
        stdout.seek(0)
        return ujson.load(stdout)


def run_arena(map_name, punter_shells, output_prefix):
    map_path = os.path.join(BASE_DIR, 'maps/%s.json' % map_name)
    commands = [shlex.split(shell) for shell in punter_shells]
    args = [
        'python3',
        'arena/offline_arena.py',
        '--map=%s' % map_path,
        '--commands=%s' % ujson.dumps(commands),
    ]
    return run_common(args, output_prefix)


def run_stadium(map_name, punter_shells, output_prefix):
    map_path = os.path.join(BASE_DIR, 'maps/%s.json' % map_name)
    args = [
        'bazel-bin/stadium/stadium',
        '--logtostderr',
        '--map=%s' % map_path,
    ] + punter_shells
    return run_common(args, output_prefix)


def run(config, punters, output_prefix):
    punter_shells = [p['shell'] for p in punters]
    if FLAGS.stadium:
        return run_stadium(config['map'], punter_shells, output_prefix)
    return run_arena(config['map'], punter_shells, output_prefix)


def decide_next_punters(config, punters):
    all_names = [p['name'] for p in punters]
    num_games_map = {name: config['num_games'] for name in all_names}
    index = load_result_index(config)
    for result in index['results']:
        for p in result['punters']:
            if p in num_games_map:
                num_games_map[p] -= 1
    eligible_names = [
        name for name in all_names if num_games_map[name] > 0]
    ineligible_names = list(set(all_names) - set(eligible_names))
    if not eligible_names:
        return None
    random.shuffle(eligible_names)
    random.shuffle(ineligible_names)
    next_names = eligible_names[:config['num_punters']]
    if len(next_names) < config['num_punters']:
        num_needed_punters = config['num_punters'] - len(next_names)
        next_names.extend(ineligible_names[:num_needed_punters])
    if len(next_names) < config['num_punters']:
        logging.error('Can not satisfy constraint for config %s',
                      config['name'])
        return None
    punter_map = {p['name']: p for p in punters}
    return [punter_map[name] for name in next_names]


def process_config(config, all_punters):
    while True:
        punters = decide_next_punters(config, all_punters)
        if not punters:
            break
        hash = binascii.hexlify(os.urandom(16))
        logging.info('Match %s: %s [%s]',
                     hash,
                     config['name'],
                     ', '.join(p['name'] for p in punters))
        result_dir = os.path.join(DATA_DIR, config['name'])
        if not os.path.exists(result_dir):
            os.makedirs(result_dir)
        output_prefix = os.path.join(result_dir, hash)

        start_time = time.time()
        try:
            result = run(config, punters, output_prefix)
        except Exception:
            result = {
                'error': 'server crashed',
                'moves': [],
                'scores': [0 for _ in punters],
            }
            logging.exception('server crashed')
        end_time = time.time()

        save_result(result, config, punters, output_prefix, start_time, end_time)
        logging.info('score: %r', result['scores'])
        check_update()


def main(unused_argv):
    setup_logging()
    build_requirements()
    settings = load_settings()
    all_punters = settings['punters']
    all_configs = settings['configurations']
    while True:
        check_update()
        for config in all_configs:
            process_config(config, all_punters)
        if not FLAGS.auto_restart:
            break
        logging.info('Sleeping...')
        time.sleep(30)


if __name__ == '__main__':
    sys.exit(main(FLAGS(sys.argv)))
