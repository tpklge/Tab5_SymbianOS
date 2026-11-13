#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate original unsigned SIS fixtures and authoring sources; no device I/O."""
import argparse, hashlib, json, pathlib, shutil, zipfile, importlib.util
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('make_sis',ROOT/'tools/make_sis.py');sis=importlib.util.module_from_spec(spec);spec.loader.exec_module(sis)
def build(output):
 output=pathlib.Path(output);output.mkdir(parents=True,exist_ok=True);sources=output/'sources';packages=output/'packages';packages.mkdir(exist_ok=True);produced=[]
 def emit(name,identifier,uid,version,*,schema=1,dependencies=None,permissions=None,migration=None,broken=False,large=False,author='Tab5Symbian contributors'):
  source=sources/name;source.mkdir(parents=True,exist_ok=True);filename='counter-v2.txt' if schema>=2 else 'counter.txt'
  if identifier=='sis_counter':
   script=f'''-- SPDX-License-Identifier: MIT
local path='/data/sis_counter/{filename}'
local notice=''
function event(action)
 local value=tonumber(sys.read(path) or '0') or 0
 if action=='increment' then value=value+1;notice=sys.write(path,tostring(value)) and 'Salvo no cartão' or 'Falha ao salvar'
 elseif action=='export' then notice=sys.write('/home/Documents/sis-counter.txt',tostring(value)) and 'Exportado' or 'Permissão negada'
 elseif action=='home' or action=='back' then next_app='shell' end
 if not sys.read(path) then sys.write(path,tostring(value)) end
 return {{title='Contador SIS {version}',body='Contador: '..value..' • '..notice,buttons={{{{id='increment',label='Somar 1'}},{{id='export',label='Exportar'}},{{id='back',label='Voltar'}}}}}}
end
'''
  else:script=f"-- SPDX-License-Identifier: MIT\nfunction event(action) if action=='home' or action=='back' then next_app='shell' end return {{title='{identifier} {version}',body='Aplicação de teste SIS original',buttons={{{{id='back',label='Voltar'}}}}}} end\n"
  if identifier=='sis_counter' and 'documents' not in (permissions or []):script=script.replace(",{id='export',label='Exportar'}",'')
  if name.startswith('19-'):
   script=script.replace(" local value=tonumber"," if action=='focus' then for n=1,32 do assert(sys.write('/data/sis_counter/trial-load-'..n..'.txt',string.rep('t',65536))) end end\n local value=tonumber")
  if broken:script="-- SPDX-License-Identifier: MIT\nsys.write('/data/sis_counter/counter-v2.txt','999')\nerror('Falha intencional de teste; o sistema deve restaurar os dados anteriores')\n"
  (source/'main.lua').write_text(script);shutil.copyfile(ROOT/'LICENSE',source/'LICENSE.txt')
  meta={'id':identifier,'uid':f'0x{uid:08x}','version':version,'title':('Contador SIS' if identifier=='sis_counter' else identifier),'author':author,'license':'MIT','files':['main.lua','LICENSE.txt'],'schema':schema,'dependencies':dependencies or [],'permissions':permissions or []}
  if migration:meta['migration']=migration
  if large:
   for i,n in enumerate([1024*1024]*3+[256*1024]):
    file=f'resource-{i}.txt';(source/file).write_bytes((b'Tab5 SIS original test data\n'*(n//28+1))[:n]);meta['files'].append(file)
  (source/'sis.json').write_text(json.dumps(meta,indent=2)+'\n');sis.build(source,packages/(name+'.sis'));produced.extend([source/'sis.json']+[source/file for file in meta['files']]+[packages/(name+'.sis'),packages/(name+'.sis.json')])
 dep=lambda uid,lo='1.0.0',hi='1.9.9':{'uid':f'0x{uid:08x}','min':lo,'max':hi}
 emit('01-base-1.0.0','sis_base',0xe0002001,'1.0.0')
 emit('02-counter-1.0.0','sis_counter',0xe0002002,'1.0.0',dependencies=[dep(0xe0002001)])
 emit('03-counter-1.1.0','sis_counter',0xe0002002,'1.1.0',dependencies=[dep(0xe0002001)],permissions=['documents'])
 migration={'from':1,'rename':[['counter.txt','counter-v2.txt']],'defaults':{'note.txt':'migração concluída','empty.txt':''}}
 emit('04-counter-2.0.0','sis_counter',0xe0002002,'2.0.0',schema=2,dependencies=[dep(0xe0002001)],permissions=['documents'],migration=migration)
 emit('05-counter-2.1.0-broken','sis_counter',0xe0002002,'2.1.0',schema=2,dependencies=[dep(0xe0002001)],permissions=['documents'],broken=True)
 emit('06-counter-0.9.0','sis_counter',0xe0002002,'0.9.0',dependencies=[dep(0xe0002001)])
 emit('07-base-2.0.0-incompatible','sis_base',0xe0002001,'2.0.0')
 emit('08-missing-dependency','sis_missing',0xe0002005,'1.0.0',dependencies=[dep(0xe0002fff)])
 emit('09-cycle-a-1.0.0','sis_cycle_a',0xe0002003,'1.0.0')
 emit('10-cycle-b-1.0.0','sis_cycle_b',0xe0002004,'1.0.0',dependencies=[dep(0xe0002003,'1.0.0','9.0.0')])
 emit('11-cycle-a-2.0.0','sis_cycle_a',0xe0002003,'2.0.0',dependencies=[dep(0xe0002004)])
 emit('12-uid-collision','sis_collision',0xe0002001,'1.0.0')
 emit('13-publisher-change','sis_counter',0xe0002002,'3.0.0',schema=2,author='Other declared publisher')
 emit('14-migration-conflict','sis_counter',0xe0002002,'3.0.0',schema=3,dependencies=[dep(0xe0002001)],migration={'from':2,'rename':[['counter-v2.txt','note.txt']],'defaults':{}})
 emit('15-counter-2.2.0-large','sis_counter',0xe0002002,'2.2.0',schema=2,dependencies=[dep(0xe0002001)],permissions=['documents'],large=True)
 emit('19-counter-2.3.0-slow-start','sis_counter',0xe0002002,'2.3.0',schema=2,dependencies=[dep(0xe0002001)],permissions=['documents'])
 emit('20-retained-identity-change','sis_counter',0xe00020ff,'1.0.0',dependencies=[dep(0xe0002001)])
 data=bytearray((packages/'02-counter-1.0.0.sis').read_bytes());data[len(data)//2]^=1;(packages/'16-corrupted.sis').write_bytes(data)
 (packages/'17-truncated.sis').write_bytes(data[:80])
 with zipfile.ZipFile(packages/'18-zip-renamed.sis','w',zipfile.ZIP_STORED) as z:z.writestr(zipfile.ZipInfo('main.lua',(2026,1,1,0,0,0)),'-- not SIS')
 produced.extend(packages/name for name in ['16-corrupted.sis','17-truncated.sis','18-zip-renamed.sis'])
 (output/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(output).as_posix()+'\n' for p in sorted(p for p in produced if p.parent==packages and p.suffix=='.sis')))
 (output/'LEIA-ME.txt').write_text('Pacotes originais MIT para testes físicos; todos sem assinatura. 05 falha intencionalmente; 07/08/11/12/13/14/16/17/18 cobrem recusas. Siga docs/ROTEIRO_FISICO_SIS.md. Não instale os cenários fora da ordem do roteiro no seu cartão principal.\n')
 produced.extend([output/'SHA256SUMS',output/'LEIA-ME.txt',output/'KIT-MANIFEST.json'])
 (output/'KIT-MANIFEST.json').write_text(json.dumps(sorted(p.relative_to(output).as_posix() for p in produced),indent=2)+'\n')
 return output
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('output',nargs='?',default=str(ROOT/'sist/1.0.0/sis-test-kit'));args=parser.parse_args();print(build(args.output))
