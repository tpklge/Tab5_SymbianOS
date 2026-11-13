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
function M.text(id,to,channel,text)
 if #text==0 or #text>228 or to<1 or to>0xffffffff or channel<0 or channel>7 then return nil end
 local data=M.number(1,1)..M.bytes(2,text)
 local packet=M.fixed(2,to)..M.number(3,channel)..M.bytes(4,data)..M.fixed(6,id)..M.number(9,3)..M.number(10,to~=0xffffffff and 1 or 0)
 return M.bytes(1,packet)
end
return M
