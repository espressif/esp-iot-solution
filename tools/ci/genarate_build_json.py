#!/usr/bin/env python

import json
import os
import pprint
import subprocess
import sys

# =============================================================================
# Service funcs
# =============================================================================


def get_idf_targets():
    args = [sys.executable, get_idf_path('tools/idf.py'), "--version"]
    output = subprocess.check_output(args).decode('utf-8')
    output = output.split('.')
    m = int(output[0][-1:])
    s = int(output[1][:1])
    print('- IDF Version v%d.%d' % (m, s))
    v = m * 10 + s
    if v == 44:
        TARGETS = ['esp32', 'esp32s2', 'esp32s3', 'esp32c3']
    elif v == 43:
        TARGETS = ['esp32', 'esp32s2', 'esp32c3']
    elif v == 42:
        TARGETS = ['esp32', 'esp32s2']
    else:
        TARGETS = ['esp32']
    return TARGETS


def _build_path(path, *paths):
    return str(os.path.normpath(os.path.join(path, *paths)).replace('\\', '/'))


def _unify_paths(path_list):
    return [_build_path(p) for p in path_list]


def _file2linelist(path):
    with open(path) as f:
        lines = [line.rstrip() for line in f]
    return [str(line) for line in lines]


# =============================================================================
# Test funcs
# =============================================================================


def get_idf_path(path, *paths):
    IDF_PATH = os.getenv('IDF_PATH')
    return _build_path(IDF_PATH, path, *paths)


def get_iot_solution_path(path, *paths):
    IDF_PATH = os.getenv('IOT_SOLUTION_PATH')
    return _build_path(IDF_PATH, path, *paths)


def get_json_name(target, build_system):
    return os.getenv(
        'BUILD_PATH') + '/list_' + target + '_' + build_system + '.json'


def read_json_list_from_file(file):
    f_json = open(file, 'r')
    o_list = f_json.read()
    f_json.close()
    o_list = o_list.split('\n')
    json_list = []
    for j in o_list:
        if j:
            json_list.append(json.loads(j))
    return json_list


def write_json_list_to_file(json_list, file):
    f_json = open(file, 'w')
    for a in json_list:
        f_json.write(json.dumps(a) + '\n')
    f_json.close()


def apply_targets(app_list, configs, target):
    print('- Applying targets')
    app_list_res = list(app_list)
    for p in app_list:
        for app in configs['examples']:
            if os.path.basename(p['app_dir']) == app['name']:
                support = 0
                for t in app['targets']:
                    if t == target:
                        support = 1

                if support != 1:
                    print('[%s] unsupport %s' % (app['name'], target))
                    app_list_res.remove(p)
    return app_list_res


def apply_buildsystem(app_list, configs, bs):
    print('- Applying buildsystem')
    app_list_res = list(app_list)
    for p in app_list:
        for app in configs['examples']:
            if os.path.basename(p['app_dir']) == app['name']:
                support = 0
                for b in app['buildsystem']:
                    if b == bs:
                        support = 1

                if support != 1:
                    print('[%s] unsupport %s' % (app['name'], bs))
                    app_list_res.remove(p)
    return app_list_res


def get_apps(target, build_system):
    print('- Getting paths of apps for %s with %s' % (target, build_system))
    json_file = get_json_name(target, build_system)
    args = [
        sys.executable,
        get_iot_solution_path('tools/find_apps.py'),
        # '-vv',
        '--format',
        'json',
        '--work-dir',
        os.getenv('BUILD_PATH') + '/@f/@w/@t',
        '--build-dir',
        'build',
        '--build-log',
        os.getenv('LOG_PATH') + '/@f_@w_@t_' + build_system + '.txt',
        '-p',
        os.getenv('IOT_SOLUTION_PATH') + '/examples',
        '--recursive',
        '--target',
        target,
        '--output',
        json_file,
        '--build-system',
        build_system,
        '--config',
        'sdkconfig.ci=default',
        '--config',
        'sdkconfig.ci.*=',
        '--config',
        '=default'
    ]
    output = subprocess.check_output(args).decode('utf-8')
    return json_file


def diff(first, second):
    print('- Comparing...')
    first = set(first)
    second = set(second)
    res = list(first - second) + list(second - first)
    return res


def generate_app_json(target, build_system, configs_json):
    json_file_path = get_apps(target, build_system)
    json_list = read_json_list_from_file(json_file_path)

    # apply target
    json_list = apply_targets(json_list, configs_json, target)

    # apply buildsystem
    json_list = apply_buildsystem(json_list, configs_json, build_system)

    write_json_list_to_file(json_list, json_file_path)


def main():

    # If the target is not specified in the example_config.json file,
    # it is considered that all targets and all build system are supported
    f_config = open(os.getenv('IOT_SOLUTION_PATH') + '/tools/ci/example_config.json')
    configs = f_config.read()
    configs = json.loads(configs)
    f_config.close()

    if not os.path.exists(os.getenv('BUILD_PATH')):
        os.mkdir(os.getenv('BUILD_PATH'))
    if not os.path.exists(os.getenv('LOG_PATH')):
        os.mkdir(os.getenv('LOG_PATH'))

    TARGETS = get_idf_targets()
    # make build system only support esp32
    generate_app_json("esp32", 'make', configs)
    for t in TARGETS:
        generate_app_json(t, 'cmake', configs)

    print('[ DONE ]')


if __name__ == '__main__':
    main()
