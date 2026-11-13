// SPDX-License-Identifier: MIT
async function testBlocks(html) {
 var scenario='large',queueRequests=[],deleted=[],downloads=0;
 function node(){return {children:[],append(...items){this.children.push(...items)},replaceChildren(){this.children=[]},click(){downloads++}};}
 var elements={},requests=0,committed=0,retries=0,lost=false,timeout=false,checks=0;
 function check(ok,why){checks++;if(!ok)throw Error(why);}
 ['token','file','upload','cancel','status','list','files','jobs'].forEach(id=>elements[id]=Object.assign(node(),{value:'ABCD2345',files:[],disabled:false,textContent:''}));
 var document={getElementById:id=>elements[id],createElement:()=>node()};
 var file={name:'thirty.bin',size:30*1024*1024,slice:(start,end)=>({start,end})};
 var fetch=async(url,options)=>{
  if(scenario==='manage') {
   check(options.headers['X-Tab5-Token']==='ABCD2345','file operations authenticated');
   if(url==='/files')return {ok:true,status:200,text:async()=> 'one.bin\t4\n<img>\t5\n'};
   if(options.method==='DELETE'){deleted.push(url);return {ok:true,status:200,text:async()=> 'ok'};}
   return {ok:true,status:200,blob:async()=>({})};
  }
  if(url==='/files')return {ok:true,status:200,text:async()=>''};
  if(scenario==='queue'){queueRequests.push(url);return {ok:true,status:200,text:async()=> 'ok'};}
  if(scenario==='cancel'){
   if(options.method==='DELETE'){deleted.push(url);return {ok:true,status:200,text:async()=> 'ok'};}
   throw Object.assign(Error('cancel'),{name:'AbortError'});
  }
  requests++;var body=options.body;
  check(body.end-body.start<=65536,'each request bounded at 64 KiB');
  if(body.start===65536&&!timeout){timeout=true;return {ok:false,status:408,text:async()=>'Bloco interrompido'};}
  if(body.start===committed)committed=body.end;else{check(body.end===committed,'retry refers only to last acknowledged block');retries++;}
  if(body.start===131072&&!lost){lost=true;throw Object.assign(Error('lost acknowledgement'),{name:'NetworkError'});}
  return {ok:true,status:200,text:async()=> 'Bloco confirmado'};
 };
 var Controller=function(){this.signal={};this.abort=function(){};};
 var source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
 new Function('document','fetch','AbortController','URL','setTimeout','crypto','confirm',source)(document,fetch,Controller,{createObjectURL:()=> 'blob:fixture',revokeObjectURL(){}},fn=>fn(),{getRandomValues:array=>array.fill(7)},()=>true);
 elements.file.files=[file];
 await elements.upload.onclick();
 check(committed===file.size,'30 MiB uploaded');check(requests===482&&retries===1,'timeout and lost acknowledgement each repeat one block');
 check(elements.status.textContent.includes('sucesso'),'success visible after final acknowledgement');
 check(!elements.file.disabled&&!elements.upload.disabled,'controls restored');
 check(elements.jobs.children[0].children[1].value===file.size,'progress reaches confirmed bytes');
 scenario='queue';elements.file.files=[{...file,name:'first.bin',size:4},{...file,name:'second.bin',size:8}];await elements.upload.onclick();
 check(queueRequests.length===2&&queueRequests[0].includes('first.bin')&&queueRequests[1].includes('second.bin'),'multiple files sent sequentially');
 check(elements.jobs.children.length===3,'previous upload result remains visible');
 scenario='cancel';await elements.upload.onclick();
 check(deleted.length===1&&deleted[0].includes('&session='),'cancellation releases only the upload session');
 check(elements.jobs.children[3].className==='job failed'&&elements.jobs.children[4].children[2].textContent.includes('Não enviado'),'cancel and files not sent remain visible');
 scenario='manage';await elements.list.onclick();check(elements.files.children.length===1,'unsafe filenames excluded');
 await elements.files.children[0].children[2].onclick();check(downloads===1,'download action saves file');
 await elements.files.children[0].children[3].onclick();check(deleted[1]==='/file?name=one.bin','delete action targets selected file');
 check(elements.jobs.children.length===5,'refresh/delete preserve upload history');
 return checks+' actual browser upload-block checks passed';
}
function run(argv){ObjC.import('Foundation');ObjC.import('stdlib');var html=ObjC.unwrap($.NSString.stringWithContentsOfFileEncodingError(argv[0],$.NSUTF8StringEncoding,null));testBlocks(html).then(s=>console.log(s),e=>{console.log(String(e));$.exit(1);});return 'Browser upload protocol test scheduled';}
if(typeof process!=='undefined'&&typeof require!=='undefined')testBlocks(require('fs').readFileSync(process.argv[2],'utf8')).then(console.log,e=>{console.error(e);process.exitCode=1;});
