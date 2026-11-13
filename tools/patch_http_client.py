#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build-local IDF 5.4 read fix for HTTP bodies delimited by connection close."""
from pathlib import Path
import argparse

def patch(source):
 start=source.index('int esp_http_client_read(')
 end=source.index('\nesp_err_t esp_http_client_perform(',start)
 body=source[start:end]
 old='is_data_remain = client->response->data_process < client->response->content_length;'
 new='is_data_remain = (client->response->content_length < 0 && !client->is_chunk_complete) || client->response->data_process < client->response->content_length;'
 anchor='''        if (rlen <= 0) {
            esp_log_level_t sev = ESP_LOG_WARN;'''
 replacement='''        if (rlen <= 0) {
            /* Tab5: a valid close-delimited response has no Content-Length.
             * Feed its FIN to the HTTP parser; only its completion callback
             * authorizes EOF. A timeout/reset is never treated as clean EOF. */
            if (rlen == ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN &&
                !client->response->is_chunked && client->response->content_length < 0) {
                http_parser_execute(client->parser, client->parser_settings, res_buffer->data, 0);
                if (client->is_chunk_complete) return ridx;
            }
            esp_log_level_t sev = ESP_LOG_WARN;'''
 if body.count(old)!=1 or body.count(anchor)!=1:raise ValueError('HTTP client read layout changed; review close-delimited integration')
 return '/* Tab5 build-local integration; upstream copyright/license preserved below. */\n'+source[:start]+body.replace(old,new).replace(anchor,replacement)+source[end:]
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(patch(a.source.read_text()))
