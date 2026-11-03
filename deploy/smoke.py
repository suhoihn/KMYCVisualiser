"""Check the deployed API: real parser trace, native compile, no execution."""
import json
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


def check(base):
    request = Request(base.rstrip('/') + '/api/playground',
                      data=json.dumps({'files': {'main.kmy': 'while (true) { }'},
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
    if not any(e['type'] == 'compilation' and e['compilation']['exitCode'] == 0 for e in events):
        raise RuntimeError('Native compilation did not succeed')
    if any(e['type'] in ('execution', 'error') or e.get('phase') == 'execution' for e in events):
        raise RuntimeError('Unexpected execution or runner error')
    if not events or events[-1] != {'type': 'result', 'status': 'compiled'}:
        raise RuntimeError('Expected the compile-only result')
    print('PASS: parser trace, native compilation, and no execution')


if __name__ == '__main__':
    check(sys.argv[1] if len(sys.argv) > 1 else 'http://127.0.0.1:8088')
