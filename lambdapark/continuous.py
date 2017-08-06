import datetime
import hashlib
import logging
import os
import random
import subprocess
import sys
import time

import gflags
import pymongo
import pytz
import ujson
import yaml

FLAGS = gflags.FLAGS

gflags.DEFINE_bool('oneoff', False, 'one-off.')

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LOG_DIR = os.path.join(BASE_DIR, 'lambdapark/logs')
JST = pytz.timezone('Asia/Tokyo')


def setup_logging():
    logging.basicConfig(
        level=logging.INFO,
        format=('%(asctime)s.%(msecs)03d %(levelname)s '
                '[%(filename)s:%(lineno)d] %(message)s'),
        datefmt='%Y-%m-%d %H:%M:%S')


def build_requirements():
    args = [
        'bazel',
        'build',
        '-c',
        'opt',
        '//stadium',
        '//punter',
    ]
    subprocess.check_call(args, cwd=BASE_DIR)


def load_settings():
    with open(os.path.join(BASE_DIR, 'lambdapark/settings.yaml')) as f:
        return yaml.load(f)


def ensure_index(db):
    db.jobs.create_index([
        ('status', pymongo.ASCENDING),
    ])
    db.snapshots.create_index([
        ('snapshot', pymongo.ASCENDING),
    ])
    db.labels.create_index([
        ('label', pymongo.ASCENDING),
    ])


def restart():
    os.execv(sys.executable, [sys.executable] + sys.argv)


def get_revision():
    return subprocess.check_output(['git', 'rev-parse', 'HEAD']).strip()


def wait_for_idle(db):
    while True:
        job = db.jobs.find_one({'status': 'pending'})
        if job:
            continue
        job = db.jobs.find_one({'status': 'running'})
        if job:
            continue
        break


def get_snapshot():
    try:
        with open(os.path.join(BASE_DIR, 'bazel-bin/punter/punter')) as f:
            punter_bin = f.read()
    except OSError:
        return None
    punter_hash = hashlib.md5(punter_bin).hexdigest()

    with open(os.path.join(BASE_DIR, 'lambdapark/settings.yaml')) as f:
        settings_text = f.read()
    settings_hash = hashlib.md5(settings_text).hexdigest()
    return punter_hash + settings_hash


def maybe_schedule(db):
    snapshot = get_snapshot()
    if not snapshot:
        logging.error('failed to build punter')
        return
    if db.snapshots.find_one({'snapshot': snapshot}):
        logging.info('snapshot %s already exists', snapshot)
        return

    timestamp = datetime.datetime.now(pytz.utc).astimezone(JST).strftime(
        '%Y%m%d-%H%M%S')
    label = 'continuous-%s-%s' % (timestamp, get_revision()[:10])

    logging.info('scheduling jobs for %s', label)

    settings = load_settings()
    punters = []
    for p in settings['punters']:
        if p.get('continuous'):
            punters.append(p['name'])

    maps_map = {m['name']: m for m in settings['maps']}

    configs = settings['continuous_configs'][:]
    configs.sort(key=lambda c: (maps_map[c['map']]['num_sites'], c['num_punters']))

    jobs = []
    for config in configs:
        selects = {p: 0 for p in punters}
        for _ in range(config['num_rounds']):
            min_selects = min(selects.values())
            min_punters = [p for p in punters if selects[p] == min_selects]
            max_punters = [p for p in punters if selects[p] != min_selects]
            random.shuffle(min_punters)
            random.shuffle(max_punters)
            round_punters = min_punters[:config['num_punters']]
            if len(round_punters) < config['num_punters']:
                round_punters.extend(
                    max_punters[:(config['num_punters'] - len(round_punters))])
            random.shuffle(round_punters)
            for p in round_punters:
                selects[p] += 1
            jobs.append({
                'map': config['map'],
                'extensions': ','.join(sorted(config.get('extensions', []))),
                'punters': round_punters,
                'priority': 0,
                'user': 'continuous',
                'label': label,
                'status': 'pending',
            })
    db.jobs.insert_many(jobs)

    db.labels.update_one(
        {'label': label},
        {'$set': {'label': label}},
        upsert=True)

    db.snapshots.update_one(
        {'snapshot': snapshot},
        {'$set': {'snapshot': snapshot}},
        upsert=True)

    logging.info('scheduled %d jobs', len(jobs))


def main(unused_argv):
    setup_logging()

    db = pymongo.MongoClient().lambdapark
    ensure_index(db)
    last_revision = get_revision()
    try:
        build_requirements()
        maybe_schedule(db)
    except Exception:
        logging.exception('failed to schedule. waiting for next push...')
    if FLAGS.oneoff:
        return
    wait_for_idle(db)
    while True:
        subprocess.call(['git', 'pull'])
        next_revision = get_revision()
        if next_revision != last_revision:
            break
        time.sleep(60)
    restart()


if __name__ == '__main__':
    sys.exit(main(FLAGS(sys.argv)))
