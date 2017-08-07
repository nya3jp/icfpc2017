import datetime
import hashlib
import logging
import os
import random
import signal
import subprocess
import sys
import time

import gflags
import pymongo
import pytz
import ujson
import yaml

FLAGS = gflags.FLAGS

gflags.DEFINE_integer('workers', None, '# workers')
gflags.DEFINE_bool('cancel_pending_jobs', False, 'cancel pending jobs')
gflags.MarkFlagAsRequired('workers')

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
    returncode = subprocess.call(args, cwd=BASE_DIR)
    return returncode == 0


def ensure_index(db):
    db.jobs.create_index([
        ('status', pymongo.ASCENDING),
    ])


def restart():
    os.execv(sys.executable, [sys.executable] + sys.argv)


def get_revision():
    return subprocess.check_output(['git', 'rev-parse', 'HEAD']).strip()


def is_idle(db):
    job = db.jobs.find_one({'status': {'$in': ['pending', 'running']}})
    return not job


def wait_idle(db):
    while not is_idle(db):
        time.sleep(5)


def run_scheduler():
    args = [
        sys.executable,
        os.path.join(BASE_DIR, 'lambdapark', 'scheduler.py'),
    ]
    subprocess.call(args, cwd=BASE_DIR)


def spawn_workers():
    procs = []
    for i in range(FLAGS.workers):
        args = [
            'taskset',
            '-c',
            '%d' % i,
            sys.executable,
            os.path.join(BASE_DIR, 'lambdapark', 'worker.py'),
        ]
        proc = subprocess.Popen(args, cwd=BASE_DIR)
        procs.append(proc)
    return procs


def terminate_workers(procs):
    for proc in procs:
        proc.send_signal(signal.SIGINT)
    time.sleep(3)
    for proc in procs:
        proc.kill()
    for proc in procs:
        proc.wait()


def run_workers(db):
    try:
        procs = spawn_workers()
        wait_idle(db)
    except KeyboardInterrupt:
        logging.info('SIGINT received. terminating workers...')
        raise
    finally:
        terminate_workers(procs)


def main(unused_argv):
    setup_logging()

    init_revision = get_revision()
    logging.info('init revision: %s', init_revision)

    db = pymongo.MongoClient().lambdapark
    ensure_index(db)

    if FLAGS.cancel_pending_jobs:
        db.jobs.delete_many({'status': {'$in': ['pending', 'running']}})
        logging.info('cleared all pending jobs')
        time.sleep(3)

    if not is_idle(db):
        logging.info('there are already pending jobs, process them first')
        run_workers(db)

    assert is_idle(db)

    subprocess.call(['git', 'pull'])

    if get_revision() != init_revision:
        logging.info('update found, restarting!')
        restart()

    if not build_requirements():
        logging.error('BUILD FAILED!')
    else:
        logging.info('running scheduler')
        run_scheduler()
        logging.info('processing jobs')
        run_workers(db)
        logging.info('jobs complete!')

    logging.info('waiting for update...')

    while True:
        subprocess.call(['git', 'pull'])
        if get_revision() != init_revision:
            restart()
        time.sleep(60)


if __name__ == '__main__':
    sys.exit(main(FLAGS(sys.argv)))
