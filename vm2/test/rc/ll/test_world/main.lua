pfm.class("World")
assert(World._name_ == "World")
function World:new()
end
function World:test_function(map, array, n, str)
	-- print("map[a] = " .. map['key_a'] .. " array[1] = " .. array[1] .. " n = " .. n .. " #str = " .. #str)
	return map['key_a'] + array[1] + n + #str 
end

pfm.class("Item")
assert(Item._name_ == "Item")
function Item:new(attack,defense,hp,mp,hit_rate,dodge_rate,speed)
	assert(attack == 160 and defense == 80)
	self.attack_point = attack
	self.defense_point = defense
	self.hp = hp
	self.mp = mp
	self.hit_rate = hit_rate
	self.dodge_rate = dodge_rate
	self.speed = speed
	return self
end
function Item:get_damage()
	assert(self.attack_point == 160 and self.defense_point == 80)
	return self.attack_point - self.defense_point
end
function Item:calc_value(bonus1, bonus2)
	--print("datas", self.hp, self.mp, self.dodge_rate, self.hit_rate, self.speed, bonus1, bonus2)
	assert(self.attack_point == 160 and self.defense_point == 80 and
		self.hp == 8 and self.mp == 9 and self.dodge_rate == 20 and self.hit_rate == 10 and
		self.speed == 1 and bonus1 == 10 and bonus2 == 20)
	return self.attack_point + self.defense_point +
		self.hp + self.mp + self.dodge_rate + self.hit_rate + self.speed + 
		bonus1 + bonus2
end

pfm.class("Player")
assert(Player._name_ == "Player")
function Player:new(name)
	assert(name == "umegaya")
	self.name = name
	assert(self.name == "umegaya")
	self.weapon = Item:new(160, 80, 8, 9, 10, 20, 1)
	assert(getmetatable(self.weapon))
	return self
end
function Player:attack()
	assert(getmetatable(self.weapon))
	local r = self.weapon:get_damage()
	assert(r == 80)
	local s = self.weapon:calc_value(10, 20)
	assert(s == 318)
	-- print("get_damage = " .. r .. ",calc_value = " ..s)
	return s
end


