import logging
import os
import subprocess
import sys
import tempfile
import time
import traceback

import gflags
import pymongo
import ujson
import yaml

FLAGS = gflags.FLAGS

gflags.DEFINE_bool('auto_update', False, 'Enable auto update.')

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LOG_DIR = os.path.join(BASE_DIR, 'lambdapark/logs')


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
    ]
    subprocess.check_call(args, cwd=BASE_DIR)


def load_settings():
    with open(os.path.join(BASE_DIR, 'lambdapark/settings.yaml')) as f:
        return yaml.load(f)


class SelfUpdater(object):
    def __init__(self):
        self._last_check = 0

    def check_update_and_maybe_restart(self):
        if not FLAGS.auto_update:
            return
        now = time.time()
        if now - self._last_check < 180:
            return
        self._last_check = now
        last_rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'])
        subprocess.call(['git', 'pull'])
        next_rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'])
        if next_rev != last_rev:
            logging.info('Detected an update. Restarting...')
            os.execv(sys.executable, [sys.executable] + sys.argv)


def run_stadium(map_name, punter_shells, log_path):
    map_path = os.path.join(BASE_DIR, 'maps/%s.json' % map_name)
    args = [
        'bazel-bin/stadium/stadium',
        '--logtostderr',
        '--map=%s' % map_path,
        '--result_json=/dev/stdout',
        '--persistent',
    ] + punter_shells
    with tempfile.TemporaryFile() as stdout:
        with open(log_path, 'w') as stderr:
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


def process_job(job, all_punters):
    job_id = str(job['_id'])

    map_name = job['map']
    assert '/' not in map_name

    all_punters_map = {p['name']: p for p in all_punters}
    punters = []
    for name in job['punters']:
        punters.append(all_punters_map[name])
    punter_shells = [p['shell'] for p in punters]

    log_path = os.path.join(LOG_DIR, '%s.log' % job_id)

    start_time = time.time()
    try:
        result = run_stadium(map_name, punter_shells, log_path)
        result['error'] = None
    except Exception:
        result = {
            'error': 'Stadium crashed!',
            'scores': None,
            'moves': None,
        }
    end_time = time.time()

    result.update({
        'job': job.copy(),
        'start_time': start_time,
        'duration': end_time - start_time,
    })
    result['job']['_id'] = str(result['job']['_id'])

    result_path = os.path.join(LOG_DIR, '%s.json' % job_id)
    with open(result_path, 'w') as f:
        ujson.dump(result, f)

    report = {
        key: result[key]
        for key in ('job', 'error', 'scores', 'start_time', 'duration')
    }
    return report


def main(unused_argv):
    setup_logging()

    if not os.path.exists(LOG_DIR):
        os.makedirs(LOG_DIR)

    settings = load_settings()
    all_punters = settings['punters']

    db = pymongo.MongoClient().lambdapark
    db.jobs.create_index([
        ('status', pymongo.ASCENDING),
        ('priority', pymongo.DESCENDING),
        ('_id', pymongo.ASCENDING),
    ])

    build_requirements()

    updater = SelfUpdater()

    logging.info('Ready!')

    while True:
        updater.check_update_and_maybe_restart()

        job = db.jobs.find_one_and_update(
            {'status': 'pending'},
            {'$set': {'status': 'running'}},
            sort=[('priority', pymongo.DESCENDING), ('_id', pymongo.ASCENDING)])

        if not job:
            time.sleep(5)
            continue

        logging.info('JOB START: %s: map=%s, punters=[%s]',
                     job['_id'], job['map'], ', '.join(job['punters']))

        try:
            start_time = time.time()
            report = process_job(job, all_punters)
        except Exception:
            end_time = time.time()
            error = traceback.format_exc().encode('utf-8')
            report = {
                'job': job,
                'error': error,
                'scores': None,
                'start_time': start_time,
                'duration': end_time - start_time,
            }
        except BaseException:
            logging.info('Quitting. Releasing the job %s.', job['_id'])
            db.jobs.update_one(
                {'_id': job['_id']},
                {'$set': {'status': 'pending'}})
            raise

        db.reports.insert_one(report)

        if report['error']:
            logging.info('JOB ERROR: %s: %s', job['_id'], report['error'])
        else:
            logging.info('JOB SUCCESS: %s: score=%r', job['_id'], report['scores'])


if __name__ == '__main__':
    sys.exit(main(FLAGS(sys.argv)))
