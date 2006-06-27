#!/usr/bin/ruby

# Ruby script for pulling together a MacOSX app bundle.

if File.exist?("lib") then
    Dir.foreach("lib") {|x| unless x[0] == 46 then File.delete("lib/"+x) end}
else
    Dir.mkdir "lib"
end

result = `otool -L ../../gtk2_ardour/ardour.bin`
results = result.split("\n")
results.delete_at(0)
results.each do |s|
    s = s.split[0]
    # exclude frameworks, system libraries, X11 libraries, and libjack.
    unless s =~ /System|\/usr\/lib|\/usr\/X11R6|libjack/ then
        `cp #{s} lib`
    end
end

`/usr/local/bin/platypus -a 'Ardour2' -t 'Shell' -o 'None' -u 'Paul Davis' -i '/bin/sh' -V '1.0' -s 'ArDr' -I 'org.ardour.Ardour2' -f 'bin' -f 'lib' -f 'Ardour2.icns' -f 'MenuBar.nib' -f 'ProgressWindow.nib' -f 'init' -f 'openDoc' 'script' 'Ardour2.app'`