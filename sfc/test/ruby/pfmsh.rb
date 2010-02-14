require 'socket'
require 'thread'

$KCODE = 'UTF8'

class Pfmsh
	def initialize(host = 'localhost', port = 23456)
		@port = port
		@hostname = host
		@s = TCPSocket.open(@hostname, @port)
		@m = Mutex.new
		@wb = String.new
		@rb = String.new
		@msgid = 0
		@pt = Thread.new do
			while true
				sleep(3)
				nw = usec
				process_packet("0 #{nw}\n") do |s|
					log "s is ping? #{s}"
					return nil if s[0] == "0"
				end
				log "send ping! at #{nw}"
			end
		end
	end
	def log(str)
		#p str
	end
	def usec
		Time.now.tv_sec + Time.now.tv_usec * 1000 * 1000
	end
	def getmsgid
		if @msgid > 2000000000 then
			@msgid = 0
		end
		@msgid += 1
	end
	def process_packet(cmd,&handler)
		log "process packet <#{cmd}>"
		@m.synchronize do
			@wb += cmd
			log "wb = #{@wb}"
			begin
				l = @s.write(@wb)
			rescue Errno::EPIPE
				@s = TCPServer.open(@hostname,@port)
				l = @s.write(@wb)
			end
			@wb = ""
			cnt = 10000
			while cnt >= 0
				sks = IO::select([@s],nil,nil,0.1)
				break if sks == nil
				next if sks[0].size <= 0
				@rb += @s.read_nonblock(128 * 1024)
				log "recv #{@rb}"
				if pos = @rb.rindex("\n") then
					a = @rb[0..pos]
					@rb = @rb[pos+1..@rb.length - pos]
					a.split("\n").each do |s|
						handler.call(s)
					end
				end
				cnt -= 1
			end
		end
		log "exit process packet"
	end
	def getcmd(cmd, args)
		msgid = getmsgid
		prefix = args[0] =~ /(\*|[0-9]*\.[0-9]*\.[0-9]*\.[0-9])/ ? 
			args.shift.to_s + ' ' + msgid.to_s + ' ' : ''
		return "#{prefix}#{cmd} #{msgid} #{args.join(' ')}\n"
	end
	def exec(*args)
		str = getcmd('exec', args)
		p "cmd = " + str
		process_packet(str) do |s|
			r = s.split(/ /)
			return r if r[0] == "0"
			if r[0] == "exec_start" then
				next if r[2] == "ok"
				p "error: " + r[2]
				return
			end
			return if r[0] == "exec_end"
			printf r[2..r.size - 1].join(" ") + "\n"
			next
		end
	end
end

