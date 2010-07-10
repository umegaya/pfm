function load_player(p)
	local sent_msg = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	function p:recvmsg(msg)
		assert(msg == sent_msg)
	end
	for i = 1, 1, 1 do 
		p:chat(sent_msg)
	end
	print("finish!")
	return p
end

function init_object(obj, t)
end

function remote_error(from,msg)
end
