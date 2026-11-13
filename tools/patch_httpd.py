#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build-local fix for IDF HTTPD task-start socket cleanup. SDK remains unchanged."""
from pathlib import Path
import argparse
ANCHOR='''        /* Failed to launch task */
        httpd_delete(hd);
        return ESP_ERR_HTTPD_TASK;'''
REPLACEMENT='''        /* Tab5: server_init already owns three sockets at this point.
         * No server thread exists to run its normal exit cleanup. */
        close(hd->msg_fd);
        cs_free_ctrl_sock(hd->ctrl_fd);
        close(hd->listen_fd);
#if CONFIG_HTTPD_QUEUE_WORK_BLOCKING
        vSemaphoreDelete(hd->ctrl_sock_semaphore);
#endif
        httpd_delete(hd);
        return ESP_ERR_HTTPD_TASK;'''
def patch(source):
 if source.count(ANCHOR)!=1:raise ValueError('Unexpected IDF HTTPD task-failure path; review socket cleanup before building')
 return '/* Tab5 build-local integration; upstream copyright/license preserved below. */\n'+source.replace(ANCHOR,REPLACEMENT)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(patch(a.source.read_text()))
