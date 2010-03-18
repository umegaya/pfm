function load_player(player)
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
end

function init_object(object,objtype)
	print("init_object : type = " .. objtype)
	pfm.bless(object, objtype);
	if objtype == "World" then
		world = object
		world.seed = 0
		world.objs = {}
		function world:new_id()
			self.seed = self.seed + 1
			if self.seed > 60000 then
				self.seed = 1
			end
			return self.seed
		end
		function world:enter(obj)
			print("world:enter called")
			assert(self == world)
			local id = self:new_id()
			obj:init_pos(self:new_id(), 10000, 20000, 5000)
			world.objs[id] = obj
		end
	end
end
