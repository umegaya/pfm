function init(player)
function player:chat(msg)
	print(self.name .. ":" .. msg);
	return 666
end
function player:echo(v)
	self:chat("echo: v = " .. tostring(v))
	return v
end
function player:call_vfunc3(a1,a2,a3,fn)
	self:chat("call_vfunc3")
	return fn(a1,a2,a3)
end
function player:calc_table_element_sum(tbl)
	self:chat("calc_table_element_sum")
	sum = 0;
	k,v=next(tbl)
	while k do
		print("vtype = " .. type(v))
		if (type(v) == "string") then
			sum = sum + #v
		end
		if (type(v) == "number") then
			sum = sum + v
		end
		k,v=next(tbl,k)
	end
	return sum
end
end
