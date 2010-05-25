pfm.class("World")
function World:new()
	assert(self == world)
end
function World:test_function(map, array, n, str)
	return map['key_b'] + array[2] + n + #str
end

pfm.class("Player")
function Player:new()
end
