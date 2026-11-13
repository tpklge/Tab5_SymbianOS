-- SPDX-License-Identifier: MIT
local M = {}
function M.fields(iso)
	local y, m, d, h, n = iso:match("^(%d%d%d%d)%-(%d%d)%-(%d%d) (%d%d):(%d%d)")
	return y and (d .. "/" .. m .. "/" .. y .. "\n" .. h .. ":" .. n) or "\n"
end
function M.epoch(date, time)
	if date:match("^%d%d%d%d%-%d%d%-%d%d ") then
		return sys.epoch(date)
	end
	local d, m, y = date:match("^(%d%d)/(%d%d)/(%d%d%d%d)$")
	if not d or not time or not time:match("^%d%d:%d%d$") then
		return nil
	end
	return sys.epoch(y .. "-" .. m .. "-" .. d .. " " .. time)
end
return M
