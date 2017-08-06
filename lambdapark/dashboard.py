import collections
import datetime
import os
import sys

import bottle
import gflags
import paste.urlparser
import pymongo
import pytz
import ujson
import yaml

FLAGS = gflags.FLAGS

gflags.DEFINE_integer('port', 8080, 'Port number.')

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
LOG_DIR = os.path.join(BASE_DIR, 'lambdapark/logs')
JST = pytz.timezone('Asia/Tokyo')

db = None


def ensure_index():
    pass


def render_template(template_name, template_dict):
    return bottle.jinja2_template(
        template_name, template_dict,
        template_settings={'autoescape': True})


@bottle.get('/')
def index_handler():
    return matrix_handler()


@bottle.get('/matrix/')
def matrix_handler():
    label = bottle.request.query.get('label')
    query = {}
    if label:
        query['job.label'] = label
    all_reports = list(db.reports.find(query))

    bucketed_points = collections.defaultdict(list)
    for report in all_reports:
        if report['error']:
            continue
        ranking = zip(report['job']['punters'], report['scores'])
        ranking.sort(key=(lambda (punter, score): score), reverse=True)
        points = []
        for i, (_, score) in enumerate(ranking):
            if i > 0 and ranking[i][1] == score:
                points.append(points[-1])
            else:
                points.append(len(ranking) - len(points))
        for (punter, _), point in zip(ranking, points):
            key = (report['job']['map'], punter)
            bucketed_points[key].append(point)

    maps = sorted(set(map for map, _ in bucketed_points))
    punters = sorted(set(punter for _, punter in bucketed_points))

    matrix = {}
    for punter in punters:
        for map in maps:
            points = bucketed_points[(map, punter)]
            avg = float(sum(points)) / len(points) if points else None
            cell = {
                'avg': avg,
                'count': len(points),
            }
            matrix[(map, punter)] = cell

    for map in maps:
        best_avg = 0
        best_punter = None
        for punter in punters:
            cell = matrix[(map, punter)]
            avg = cell['avg']
            if avg is not None and avg > best_avg:
                best_avg = avg
                best_punter = punter
        for punter in punters:
            cell = matrix[(map, punter)]
            cell['best'] = (punter == best_punter)

    template_dict = {
        'label': label,
        'maps': maps,
        'punters': punters,
        'matrix': matrix,
    }
    return render_template('matrix.html', template_dict)


@bottle.get('/results/')
def results_handler():
    label = bottle.request.query.get('label')
    skip = int(bottle.request.query.get('skip', '0'))
    limit = int(bottle.request.query.get('limit', '100'))
    query = {}
    if label:
        query['job.label'] = label
    reports = list(db.reports.find(
        query,
        sort=[('_id', pymongo.DESCENDING)],
        skip=skip,
        limit=limit))
    for report in reports:
        start_time_utc = datetime.datetime.fromtimestamp(
            report['start_time'], pytz.utc)
        start_time_jst = start_time_utc.astimezone(JST)
        report['injected_start_time'] = start_time_jst.strftime('%m/%d %H:%M:%S')
        if not report['scores']:
            injected_sorted_punters = None
        else:
            injected_sorted_punters = [
                {
                    'name': name,
                    'score': score,
                }
                for name, score in zip(report['job']['punters'], report['scores'])
            ]
            injected_sorted_punters.sort(key=lambda p: p['score'], reverse=True)
        report['injected_sorted_punters'] = injected_sorted_punters
    template_dict = {
        'label': label,
        'reports': reports,
        'skip': skip,
        'limit': limit,
    }
    return render_template('results.html', template_dict)


def main(unused_argv):
    global db
    db = pymongo.MongoClient().lambdapark

    ensure_index()

    bottle.TEMPLATE_PATH = [os.path.join(os.path.dirname(__file__), 'templates')]
    bottle.mount(
        '/static/',
        paste.urlparser.StaticURLParser(
            os.path.join(os.path.dirname(__file__), 'static')))
    bottle.run(port=FLAGS.port, host='0.0.0.0', debug=True)


if __name__ == '__main__':
    sys.exit(main(FLAGS(sys.argv)))
