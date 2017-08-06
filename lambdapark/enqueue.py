import logging
import os
import sys

import gflags
import pymongo

FLAGS = gflags.FLAGS

gflags.DEFINE_string('user', os.getlogin(), 'User name.')
gflags.DEFINE_string('map', None, 'Map name.')
gflags.DEFINE_string('label', None, 'Label.')
gflags.DEFINE_integer('priority', 0, 'Job priority.')
gflags.MarkFlagAsRequired('map')

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))


def setup_logging():
    logging.basicConfig(
        level=logging.INFO,
        format=('%(asctime)s.%(msecs)03d %(levelname)s '
                '[%(filename)s:%(lineno)d] %(message)s'),
        datefmt='%Y-%m-%d %H:%M:%S')


def main(argv):
    setup_logging()

    punters = argv[1:]
    if not punters:
        return 'Punters not specified.'

    db = pymongo.MongoClient().lambdapark

    job = {
        'map': FLAGS.map,
        'punters': punters,
        'priority': FLAGS.priority,
        'user': FLAGS.user,
        'label': FLAGS.label,
        'status': 'pending',
    }
    db.jobs.insert(job)


if __name__ == '__main__':
    sys.exit(main(FLAGS(sys.argv)))
