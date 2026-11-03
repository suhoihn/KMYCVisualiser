"""Check the API after startup, in compile-only or trusted CI run mode."""
import argparse
import json
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


def check(base, run=False):
    source = 'print(7);' if run else 'while (true) { }'
    request = Request(base.rstrip('/') + '/api/playground',
                      data=json.dumps({'files': {'main.kmy': source},
                                       'entryFile': 'main.kmy', 'action': 'run'}).encode(),
                      headers={'Content-Type': 'application/json'})
    for attempt in range(10):
        try:
            with urlopen(request, timeout=55) as response:
                events = [json.loads(line) for line in response if line.strip()]
            break
        except HTTPError as error:
            if error.code not in (502, 503, 504) or attempt == 9:
                raise
        except URLError:
            if attempt == 9:
                raise
        time.sleep(2)
    if not any(e['type'] == 'analysis' and e['analysis']['ok'] and
               any(p['kind'] == 'createNode' for p in e['analysis']['parser']['events']) for e in events):
        raise RuntimeError('Missing successful parser trace')
    compilation = next((e['compilation'] for e in events if e['type'] == 'compilation'), None)
    if not compilation or compilation['exitCode'] != 0:
        raise RuntimeError('Native compilation did not succeed')
    if '.intel_syntax noprefix' not in compilation.get('assembly', {}).get('text', ''):
        raise RuntimeError('Missing generated Linux assembly')
    if any(e['type'] == 'error' for e in events):
        raise RuntimeError('Runner returned an error')
    executions = [e['execution'] for e in events if e['type'] == 'execution']
    expected_status = 'success' if run else 'compiled'
    if not events or events[-1] != {'type': 'result', 'status': expected_status}:
        raise RuntimeError(f'Expected final result: {expected_status}')
    if run:
        if len(executions) != 1 or executions[0]['exitCode'] != 0 or executions[0]['stdout'] != '7\n':
            raise RuntimeError('Expected the trusted smoke program to print 7')
    elif executions or any(e.get('phase') == 'execution' for e in events):
        raise RuntimeError('Unexpected execution in compile-only mode')
    print('PASS: parser trace, native compilation, and ' + ('native output' if run else 'no execution'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', nargs='?', default='http://127.0.0.1:8088')
    parser.add_argument('--run', action='store_true', help='expect native execution (trusted CI only)')
    args = parser.parse_args()
    check(args.base, run=args.run)
