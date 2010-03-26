function load_player(player)
function player:set_name(name)
	self.name = name
end
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
	local sum = 0;
	local k,v=next(tbl)
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
function player:new_item(atkbase)
	item = pfm.new("ITEM");
	item.attack_point = atkbase * 10
	print("item.atk = " .. item.attack_point)
	return item
end
function player:get_item_data(prop)
	return item[prop]
end
function player:init_pos(id,x,y,z)
	self.id = id
	self.x = x
	self.y = y
	self.z = z
	print("init world pos : " .. id .. "," .. x .. "," .. y .. "," .. z)
end
function player:enter_world()
	self:chat("enter_world")
	world:enter(self)
end
function player:get_pos()
	local p = {}
	p[1] = self.x
	p[2] = self.y
	p[3] = self.z
	return p
end
	local a = 1000
	local b = 2000
	local c = 3000
function player:_protected_call()
	return "its protected"
end
	print( "a,b,c = " .. a .. "," .. b .. "," .. c)
	assert((a + b + c) == 6000)
return player
end

function dump_table(tbl)
	local k,v=next(tbl)
	while k do
		print("t[" .. k .. "]=" .. type(v))
		k,v=next(tbl,k)
	end
end

function init_object(object,objtype)
	print("init_object : type = " .. objtype);
	pfm.bless(object, objtype);
	if objtype == "World" then
		world = object
		object.seed = 0
		object.objs = {}
		function world:new_id()
			local s = self.seed + 1
			if s > 60000 then
				self.seed = 1
			end
			return s
		end
		function world:enter(obj)
			print("world:enter called")
			assert(self == world)
			local id = self:new_id()
			obj:init_pos(self:new_id(), 10000, 20000, 5000)
			self.objs[id] = obj
		end
	end
	return object
end
