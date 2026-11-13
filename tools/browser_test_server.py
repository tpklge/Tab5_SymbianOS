#!/usr/bin/env python3
"""Original, synthetic browser fixtures for physical Tab5 tests; no device access."""
import argparse
import html
import struct
import zlib
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit


def image():
    def chunk(name, data):
        return struct.pack('>I', len(data)) + name + data + struct.pack('>I', zlib.crc32(name + data))
    rows = b''.join(b'\0' + bytes([20, 100 + y // 2, 180]) * 280 for y in range(160))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 280, 160, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass  # Never print cookies or form contents.

    def reply(self, body, content_type='text/html; charset=utf-8', cookie=None, code=200, location=None):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Connection', 'close')
        if cookie:
            self.send_header('Set-Cookie', cookie)
        if location:
            self.send_header('Location', location)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlsplit(self.path)
        if path.path == '/image.png':
            return self.reply(image(), 'image/png')
        if path.path == '/loop':
            return self.reply('<p>Texto preservado após limite de JavaScript.</p><script>while(true){}</script>')
        if path.path == '/memory':
            return self.reply("<p>Texto preservado após falta de memória JS.</p><script>var a=[];for(;;)a.push(new Array(8192).join('x'));</script>")
        if path.path == '/large':
            return self.reply('<!--' + 'x' * (2 * 1024 * 1024) + '--><main><h1>Página de 2 MiB OK</h1><p>Conteúdo depois do comentário grande.</p></main>')
        if path.path == '/deep':
            return self.reply('<main>' + '<div>' * 3000 + '<p>Profundidade limitada</p>' + '</div>' * 3000 + '</main>')
        if path.path == '/search':
            value = parse_qs(path.query).get('q', [''])[0]
            return self.reply('<h1>GET OK</h1><p>' + html.escape(value) + '</p>')
        if path.path == '/session':
            cookies = SimpleCookie(self.headers.get('Cookie', ''))
            ok = cookies.get('tab5_demo') and cookies['tab5_demo'].value == 'fixture'
            return self.reply('<h1>' + ('Sessão OK' if ok else 'Sem sessão') + '</h1>')
        if path.path == '/logout':
            return self.reply('<p>Sessão removida</p>', cookie='tab5_demo=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax')
        self.reply('''<!doctype html><html><head><title>Teste físico R2</title>
<style>p{color:blue;padding:4px}h1{color:green}</style></head><body><main>
<h1>Teste do navegador</h1><p id="js">Texto inicial</p>
<script>document.getElementById('js').textContent='JavaScript básico OK';</script>
<img src="/image.png" alt="Imagem azul de teste">
<p><a href="/large">Página de 2 MiB</a></p><p><a href="/deep">Estrutura profunda</a></p>
<p><a href="/loop">Laço JS</a></p><p><a href="/memory">Memória JS</a></p>
<p><a href="/session">Conferir sessão</a></p><p><a href="/logout">Encerrar sessão</a></p>
<form action="/search" method="get"><input name="q" placeholder="Busca de teste"></form>
<form action="/login" method="post"><input name="nome" placeholder="Nome fictício"></form>
</main></body></html>''')

    def do_POST(self):
        if urlsplit(self.path).path != '/login':
            return self.reply('Destino desconhecido', code=404)
        length = int(self.headers.get('Content-Length', '0'))
        if not 0 <= length <= 16384:
            return self.reply('Limite', code=413)
        self.rfile.read(length)  # Fixture discards the supplied value immediately.
        self.reply('', cookie='tab5_demo=fixture; Path=/; HttpOnly; SameSite=Lax', code=303, location='/session')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', default='127.0.0.1')
    parser.add_argument('--port', default=8765, type=int)
    args = parser.parse_args()
    print(f'Fixtures HTTP em {args.bind}:{args.port}; Ctrl+C encerra. Use somente dados fictícios.', flush=True)
    ThreadingHTTPServer((args.bind, args.port), Handler).serve_forever()
