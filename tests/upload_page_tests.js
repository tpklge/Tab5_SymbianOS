// SPDX-License-Identifier: MIT
function testPage(html) {
  var checks=0;
  function check(ok,message){checks++;if(!ok)throw Error(message);}
  var elements={},calls=0;
  ['token','file','upload','cancel','status','list','files','jobs'].forEach(function(id){elements[id]={value:'ABCD2345',files:[],disabled:false,textContent:'',replaceChildren:function(){},append:function(){}};});
  var document={getElementById:function(id){return elements[id];},createElement:function(){return {append:function(){},click:function(){}};}};
  var fetch=function(){calls++;return Promise.resolve({ok:true,status:200,text:function(){return Promise.resolve('');}});};
  var Controller=function(){this.signal={};this.abort=function(){};};
  var source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
  var api=new Function('document','fetch','AbortController','URL','setTimeout',source+';return {fileError:fileError,limit:MAX_UPLOAD_BYTES};')(document,fetch,Controller,{createObjectURL:function(){return '';},revokeObjectURL:function(){}},function(){});
  check(api.limit===67108864,'upload limit is exactly 64 MiB');
  elements.file.files=[{name:'grande.bin',size:100*1000*1000}];elements.file.onchange();elements.upload.onclick();
  check(calls===0,'100 MB must never start fetch');check(elements.upload.disabled,'oversized upload button disabled');check(elements.status.textContent.indexOf('64 MiB')>=0&&elements.status.textContent.indexOf('Nada foi enviado')>=0,'clear oversized-file message');
  check(api.fileError({name:'limite.bin',size:api.limit})==='','exact maximum accepted');check(api.fileError({name:'acima.bin',size:api.limit+1}).indexOf('64 MiB')>=0,'one byte above limit rejected');
  check(api.fileError({name:'vazio.bin',size:0}).indexOf('vazio')>=0,'empty file message');check(api.fileError({name:'nome invalido.bin',size:10}).indexOf('Nome inválido')>=0,'invalid filename message');
  elements.file.files=[{name:'correto.bin',size:100}];elements.file.onchange();check(!elements.upload.disabled,'valid new selection reenables upload');check(calls===0,'selection does not upload automatically');
  return checks+' actual upload-page checks passed';
}
function run(argv){ObjC.import('Foundation');var text=ObjC.unwrap($.NSString.stringWithContentsOfFileEncodingError(argv[0],$.NSUTF8StringEncoding,null));return testPage(text);}
if(typeof process!=='undefined'&&typeof require!=='undefined')console.log(testPage(require('fs').readFileSync(process.argv[2],'utf8')));
