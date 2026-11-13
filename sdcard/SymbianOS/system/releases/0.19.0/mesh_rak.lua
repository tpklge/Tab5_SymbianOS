-- SPDX-License-Identifier: MIT
local app=sys.module("mesh_ui")
function event(action,value) return app.view("uart",action,value) end
