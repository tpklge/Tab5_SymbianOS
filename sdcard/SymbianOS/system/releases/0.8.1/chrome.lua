-- SPDX-License-Identifier: MIT
return {
	view = function(v)
		v.left = v.left or "Opções"
		v.right = v.right or "Voltar"
		v.home, v.menu, v.switcher, v.keyboard = "Início", "Menu", "Abertos", "Teclado"
		v.background, v.accent, v.foreground, v.fieldColor = "26292e", "3c4652", "f2f3f5", "30353c"
		if not v.layout then
			v.layout = v.list and "list" or v.field and "editor" or "list"
		end
		for key, value in pairs(sys.theme()) do
			v[key] = value
		end
		return v
	end,
}
