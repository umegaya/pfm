pfm.class("World")
function World:new()
	print("World:new called")
	self.seed = 664
	return self
end
function World:test_function(map, array, n, str)
	return map['key_b'] + array[2] + n + #str
end
function World:get_id()
	print("World:get_id called : seed = " .. self.seed)
	self.seed = self.seed + 1
	print("World:get_id now seed = " .. self.seed)
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
	assert(self.hoge == 123)
	return self.id
end
function Player:login()
	return self.hoge + self.id -- should be 123 + 665 => 788
end
