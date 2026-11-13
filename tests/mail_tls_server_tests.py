# SPDX-License-Identifier: MIT
"""Local real TLS server. Temporary keys never enter SD/distribution."""
import pathlib, socket, ssl, subprocess, sys, tempfile, threading, time
binary=sys.argv[1]
with tempfile.TemporaryDirectory(prefix='tab5-real-tls-') as directory:
    root=pathlib.Path(directory)
    def cert(name):
        subprocess.run(['openssl','req','-x509','-newkey','rsa:2048','-nodes','-days','2','-keyout',str(root/(name+'.key')),'-out',str(root/(name+'.pem')),'-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost','-addext','basicConstraints=critical,CA:TRUE'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    cert('server');cert('other')
    for mode in ('ok','starttls','reject','hostname','cancel','timeout','imap','imap-stall','smtp','smtp-drop'):
        listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(5)
        errors=[]
        def serve():
            try:
                conn,_=listener.accept()
                with conn:
                    conn.settimeout(5)
                    if mode in ('cancel','timeout'): time.sleep(.4);return
                    if mode=='starttls':
                        data=b''
                        while not data.endswith(b'\r\n'): data+=conn.recv(1)
                        assert data==b'STARTTLS\r\n';conn.sendall(b'220 ready\r\n')
                    # Inspect the real ClientHello, not a mocked configuration call.
                    hello=b'';until=time.monotonic()+3
                    while time.monotonic()<until:
                        hello=conn.recv(65536,socket.MSG_PEEK)
                        if len(hello)>=5 and len(hello)>=5+int.from_bytes(hello[3:5],'big'):break
                        time.sleep(.001)
                    assert hello[0]==22 and hello[5]==1
                    pos=43;pos+=1+hello[pos]
                    n=int.from_bytes(hello[pos:pos+2],'big');pos+=2
                    suites=[int.from_bytes(hello[i:i+2],'big') for i in range(pos,pos+n,2)];pos+=n
                    assert set(suites)-{255}=={0xc02f,0xc02b,0xc030,0xc02c},suites
                    pos+=1+hello[pos];n=int.from_bytes(hello[pos:pos+2],'big');pos+=2;end=pos+n;extensions={}
                    while pos<end:
                        kind=int.from_bytes(hello[pos:pos+2],'big');n=int.from_bytes(hello[pos+2:pos+4],'big');pos+=4
                        extensions[kind]=hello[pos:pos+n];pos+=n
                    assert extensions.get(1)==b'\x02','MFL 1024 missing'
                    assert extensions.get(10)==b'\x00\x04\x00\x17\x00\x18','curve profile differs'
                    assert extensions.get(13)==b'\x00\x08\x04\x01\x05\x01\x04\x03\x05\x03','signature profile differs'
                    if mode!='hostname':assert b'localhost' in extensions.get(0,b''),'SNI missing'
                    context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                    context.minimum_version=context.maximum_version=ssl.TLSVersion.TLSv1_2
                    context.load_cert_chain(root/'server.pem',root/'server.key')
                    with context.wrap_socket(conn,server_side=True) as secure:
                        if mode in ('smtp','smtp-drop'):
                            import base64
                            stream=secure.makefile('rb')
                            def command(expected):assert stream.readline(65536)==expected
                            secure.sendall(b'220 local SMTP\r\n');command(b'EHLO tab5.local\r\n')
                            secure.sendall(b'250-local\r\n250 AUTH PLAIN\r\n')
                            expected=base64.b64encode(b'\0sender\0outgoing-only-secret')
                            command(b'AUTH PLAIN '+expected+b'\r\n');secure.sendall(b'334 challenge\r\n')
                            command(expected+b'\r\n');secure.sendall(b'235 authenticated\r\n')
                            command(b'MAIL FROM:<local@example.test>\r\n');secure.sendall(b'250 sender\r\n')
                            command(b'RCPT TO:<alice@example.test>\r\n');secure.sendall(b'251 forwarded\r\n')
                            command(b'DATA\r\n');secure.sendall(b'354 ready\r\n')
                            body=b''
                            while True:
                                line=stream.readline(65536)
                                assert line,'SMTP DATA interrupted'
                                if line==b'.\r\n':break
                                body+=line
                            assert b'Message-ID: <integration@tab5.local>' in body
                            if mode=='smtp':secure.sendall(b'250 queued\r\n')
                            stream.close()
                            return
                        if mode in ('imap','imap-stall'):
                            secure.sendall(b'* OK local IMAP ready\r\n')
                            stream=secure.makefile('rb')
                            def command(expected):
                                line=stream.readline(65536)
                                assert line==expected,(line,expected)
                            command(b'T1 CAPABILITY\r\n');secure.sendall(b'* CAPABILITY IMAP4rev1\r\nT1 OK cap\r\n')
                            command(b'T2 LOGIN "reader" "incoming-only-secret"\r\n')
                            if mode=='imap-stall':time.sleep(.4);return
                            secure.sendall(b'T2 OK login\r\n')
                            command(b'T3 LIST "" "*"\r\n');secure.sendall(b'* LIST () "/" "INBOX"\r\nT3 OK list\r\n')
                            command(b'T4 SELECT "INBOX"\r\n');secure.sendall(b'* OK [UIDVALIDITY 77] valid\r\nT4 OK selected\r\n')
                            command(b'T5 UID SEARCH ALL\r\n');secure.sendall(b'* SEARCH 7\r\nT5 OK search\r\n')
                            command(b'T6 UID FETCH 7 (RFC822.SIZE)\r\n');secure.sendall(b'* 1 FETCH (RFC822.SIZE 51000)\r\nT6 OK size\r\n')
                            command(b'T7 UID FETCH 7 (BODY.PEEK[]<0.1048577>)\r\n')
                            raw=b'From: local@example.test\r\nSubject: Integration\r\n\r\n'+b'x'*50000
                            response=b'* 1 FETCH (BODY[] {'+str(len(raw)).encode()+b'}\r\n'+raw+b'\r\n)\r\nT7 OK fetched\r\n'
                            for offset in range(0,len(response),512):
                                secure.sendall(response[offset:offset+512]);time.sleep(.005)
                            return
                        while True:
                            data=secure.recv(4096)
                            if not data:break
                            secure.sendall(data)
            except ssl.SSLError:
                if mode not in ('reject','hostname'): errors.append('unexpected TLS error')
            except Exception as exc:errors.append(str(exc))
            finally: listener.close()
        thread=threading.Thread(target=serve);thread.start()
        try:
            subprocess.run([binary,'127.0.0.1' if mode=='hostname' else 'localhost',str(listener.getsockname()[1]),str(root/('other.pem' if mode=='reject' else 'server.pem')),'reject' if mode=='hostname' else mode],check=True,timeout=10)
        finally: thread.join(6)
        assert not thread.is_alive() and not errors,errors
print('Real TLS 1.2: valid chain, wrong CA, hostname mismatch, STARTTLS, fragmented 32 KiB echo, stalled peer cancellation passed')
