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
    return subprocess.check_output(['git', 'rev-parse', 'HEAD'])


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
    subprocess.call(
        ['bazel', 'build', '-c', 'opt', '//punter'],
        cwd=BASE_DIR)
    try:
        with open(os.path.join(BASE_DIR, 'bazel-bin/punter/punter')) as f:
            punter_bin = f.read()
    except OSError:
        return None
    return hashlib.md5(punter_bin).hexdigest()


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
    label = 'continuous-%s-%s' % (timestamp, get_revision())

    logging.info('scheduling jobs for %s', label)

    settings = load_settings()
    punters = []
    for p in settings['punters']:
        if p.get('continuous'):
            punters.append(p['name'])

    jobs = []
    for config in settings['continuous_configs']:
        for _ in range(config['num_rounds']):
            random.shuffle(punters)
            round_punters = punters[:config['num_punters']]
            jobs.append({
                'map': config['map'],
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
    maybe_schedule(db)
    if FLAGS.oneoff:
        return
    last_revision = get_revision()
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
