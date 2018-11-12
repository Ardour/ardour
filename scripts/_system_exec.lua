ardour { ["type"] = "EditorAction", name = "System Exec" }

function factory () return function ()
	-- ** EXAMPLES TO RUN EXTERNAL APPLICATIONS ** --

	-- run a command in a shell and wait for it to complete.
	--
	-- Details: basically just system(3), except on Unix like systems with
	-- memory-locking, this call is special-cased to use vfork and close
	-- file-descriptors. On other systems it defaults to Lua's os-library
	-- built-in os.execute system() call.
	os.execute ("date > /tmp/testdate")

	-- os.forkexec() works as fire-and-forget. execv(3) style
	--
	-- Details: It calls vfork() and exec() under the hood, passing each
	-- argument separately to exec (and needs a full-path to the binary).
	os.forkexec ("/usr/bin/xterm")
	os.forkexec ("/bin/sh", "-c", "import --frame \"/tmp/scr_$(date).png\"")
end end
