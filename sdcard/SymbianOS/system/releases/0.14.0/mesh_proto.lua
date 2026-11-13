-- SPDX-License-Identifier: MIT
-- Original bounded subset of the public Meshtastic protobuf wire protocol.
local M={}
local function varint(n)
 local b={};repeat local v=n&127;n=n>>7;b[#b+1]=string.char(v|(n>0 and 128 or 0)) until n==0;return table.concat(b)
end
function M.number(tag,n) return varint(tag<<3)..varint(n) end
function M.bytes(tag,s) return varint((tag<<3)|2)..varint(#s)..s end
function M.fixed(tag,n) return varint((tag<<3)|5)..string.pack('<I4',n&0xffffffff) end
function M.decode(s)
 if type(s)~="string" or #s>512 then return nil end
 local out,pos={},1
 local function readvar()
  local v=0
  for shift=0,63,7 do local b=s:byte(pos);if not b then return nil end;pos=pos+1;if shift==63 and b>1 then return nil end;v=v|((b&127)<<shift);if b<128 then return v end end
 end
 while pos<=#s do
  local key=readvar();if not key or key<8 then return nil end
  local tag,wire=key>>3,key&7;local value
  if wire==0 then value=readvar()
  elseif wire==5 then if pos+3>#s then return nil end;value=string.unpack('<I4',s,pos);pos=pos+4
  elseif wire==1 then if pos+7>#s then return nil end;value=s:sub(pos,pos+7);pos=pos+8
  elseif wire==2 then local n=readvar();if not n or n<0 or n>512 or pos+n-1>#s then return nil end;value=s:sub(pos,pos+n-1);pos=pos+n
  else return nil end
  if value==nil then return nil end;out[tag]=value
 end
 return out
end
-- Rebuild selected fields while retaining all other wire bytes unchanged.
function M.replace(s,replacements)
 local parts,pos={},1
 local function v() local n,shift=0,0;repeat local b=s:byte(pos);if not b or shift>63 then return nil end;pos=pos+1;n=n|((b&127)<<shift);shift=shift+7;if b<128 then return n end until false end
 while pos<=#s do
  local start=pos;local k=v();if not k then return nil end;local tag,wire=k>>3,k&7
  if wire==0 then if not v() then return nil end elseif wire==1 then pos=pos+8 elseif wire==5 then pos=pos+4 elseif wire==2 then local n=v();if not n then return nil end;pos=pos+n else return nil end
  if pos>#s+1 then return nil end
  if not replacements[tag] then parts[#parts+1]=s:sub(start,pos-1) end
 end
 for tag=1,128 do if replacements[tag] then parts[#parts+1]=replacements[tag] end end
 return table.concat(parts)
end
function M.admin(id,node,payload,response)
 local data=M.number(1,6)..M.bytes(2,payload)..M.number(3,response and 1 or 0)
 return M.bytes(1,M.fixed(2,node)..M.bytes(4,data)..M.fixed(6,id))
end
function M.float(n) if type(n)~='number' then return nil end;local f=string.unpack('<f',string.pack('<I4',n&0xffffffff));if f~=f or math.abs(f)>1e12 then return nil end;return f end
function M.signed(n) return n>=0x80000000 and n-0x100000000 or n end
function M.text(id,to,channel,text)
 if #text==0 or #text>228 or to<1 or to>0xffffffff or channel<0 or channel>7 then return nil end
 local data=M.number(1,1)..M.bytes(2,text)
 local packet=M.fixed(2,to)..M.number(3,channel)..M.bytes(4,data)..M.fixed(6,id)..M.number(9,3)..M.number(10,to~=0xffffffff and 1 or 0)
 return M.bytes(1,packet)
end
return M
