pfm.class("World")
function World:new()
	self.seed = 665
	return self
end
function World:test_function(map, array, n, str)
	return map['key_b'] + array[2] + n + #str
end
function World:get_id()
	print("World:get_id called")
	self.seed = self.seed + 1
	return self.seed
end

pfm.class("Player")
function Player:new()
	assert(world)
	assert(getmetatable(world))
	print("call world:get_id!")
	self.hoge = 123
	self.id = world:get_id()
	return self
end
function Player:get_id()
	return self.id
end
