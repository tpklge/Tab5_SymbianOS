# SPDX-License-Identifier: MIT
import http.cookiejar
import importlib.util
import pathlib
import threading
import urllib.parse
import urllib.request

root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('fixtures', root / 'tools/browser_test_server.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
server = fixture.ThreadingHTTPServer(('127.0.0.1', 0), fixture.Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
base = f'http://127.0.0.1:{server.server_port}'
try:
    cookies = http.cookiejar.CookieJar()
    client = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(cookies))
    def get(path):
        with client.open(base + path, timeout=5) as response:
            return response.read()
    assert b'getElementById' in get('/')
    assert b'GET OK' in get('/search?q=teste')
    with client.open(base + '/login', data=urllib.parse.urlencode({'nome': 'Ficticio'}).encode(), timeout=5) as response:
        assert 'Sessão OK' in response.read().decode()
    assert len(cookies) == 1
    assert get('/image.png').startswith(b'\x89PNG\r\n\x1a\n')
    assert len(get('/large')) > 2 * 1024 * 1024
    assert b'while(true)' in get('/loop')
    get('/logout')
    assert 'Sem sessão' in get('/session').decode()
finally:
    server.shutdown()
    server.server_close()
    thread.join(timeout=3)
print('Physical test fixtures: GET, POST 303, cookie session, logout, PNG and large page passed')
