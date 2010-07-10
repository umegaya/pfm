function load_player(player)
function player:chat(msg)
	world:notify_chat(msg, self.id)
end
function player:set_id(id)
	self.id = id
end
function player:recvmsg(msg)
	assert(#msg == 128)
	return true
end
	world:enter(player)
	return player
end

function init_object(object,objtype)
	pfm.bless(object, objtype);
	if objtype == "World" then
		object.seed = 0
		object.objs = {}
		object.cnt = 0
		function object:new_id()
			local s = self.seed + 1
			if s > 60000 then
				self.seed = 1
			else
				self.seed = s
			end
			-- print("new_id = " .. s)
			return s
		end
		function object:notify_chat(msg, id)
			-- print("id = " .. id .. " msg = " .. msg .. " objs = " .. #self.objs)
			local hash = id % 50
			-- print("hash = " .. hash)
			for i = id, #self.objs, 50 do
				-- print("idx = " .. i)
				self.objs[i]:recvmsg(msg)
			end
		end
		function object:enter(obj)
			-- print("world:enter called")
			-- assert(self == world)
			local id = self:new_id()
			print("enter : id = " .. id)
			obj:set_id(id)
			self.objs[id] = obj
			assert(pfm.typeof(self.objs[id]) == "Player")
		end
	end
	return object
end
