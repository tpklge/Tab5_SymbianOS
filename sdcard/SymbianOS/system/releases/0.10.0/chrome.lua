-- SPDX-License-Identifier: MIT
return {
	view = function(v)
		v.left = v.left or "Opções"
		v.right = v.right or "Voltar"
		v.home, v.menu, v.switcher, v.keyboard = "Início", "Menu", "Abertos", "Teclado"
        local preferences=sys.theme()
        local themes=sys.module("themes")
        local theme=themes[preferences.builtin or "aurora"] or themes.aurora
        for key,value in pairs(theme) do if key~="name" then v[key]=value end end
        for key,value in pairs(preferences) do if key~="builtin" then v[key]=value end end
        if not v.layout then v.layout=v.list and "list" or v.field and "editor" or "list" end
		return v
	end,
}
