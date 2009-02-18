#!/usr/bin/env ruby

# Ruby script for pulling together a MacOSX app bundle.

# it will be either powerpc or i386
versionline = `grep -m 1 '^ardour_version =' ../../SConstruct`
version = versionline.split(" = ")[1].chomp().slice(1..-2)
$stdout.printf("Version is %s\n", version)

arch = `uname -p`.strip()
libdir = "lib_" + arch
bindir = "bin_" + arch

ppc_libdir = "lib_powerpc"
i386_libdir = "lib_i386"
ppc_bindir = "bin_powerpc"
i386_bindir = "bin_i386"
ppc_binlib = "binlib_powerpc.zip"
i386_binlib = "binlib_i386.zip"

#$stdout.print("libdir is '" + libdir + "'\n")

# check for the other arch's libbin.zip
if arch == "i386" then
  zipfile = ppc_binlib
  `rm -rf #{ppc_libdir} #{ppc_bindir}`
else
  zipfile = i386_binlib
  `rm -rf #{i386_libdir} #{i386_bindir}`
end

if File.exist?(zipfile) then
  $stdout.print("Found #{zipfile} : unpacking...\n")
  `unzip -aq #{zipfile}`
end


if File.exist?(libdir) then
  # remove it
  `rm -rf #{libdir}/*`
  #Dir.foreach(libdir) {|x| unless ( x[0] == 46 or File.stat(libdir+"/"+x).directory?) then File.delete(libdir + "/" +x) end}
else
    Dir.mkdir libdir
end

if File.exist?(bindir) then
    Dir.foreach(bindir) {|x| unless x[0] == 46 then File.delete(bindir + "/" +x) end}
else
    Dir.mkdir bindir
end

if not File.exist?(libdir+"/surfaces") then
   Dir.mkdir(libdir + "/surfaces")
end

if not File.exist?(libdir+"/vamp-plugins") then
   Dir.mkdir(libdir + "/vamp-plugins")
end


odir = Dir.getwd
Dir.chdir("../..")

result = `otool -L gtk2_ardour/ardour-#{version}`
results = result.split("\n")
results.delete_at(0)

result =  `otool -L libs/ardour/libardour.dylib`
results = results + result.split("\n").slice(1,result.size-1)

result =  `otool -L libs/surfaces/*/*.dylib`
results = results + result.split("\n").slice(1,result.size-1)

result =  `otool -L libs/vamp-plugins/*.dylib`
results = results + result.split("\n").slice(1,result.size-1)

results.uniq!

$stdout.print("Copying libs to #{libdir} ...\n");

results.each do |s|
    s = s.split[0]
    # exclude frameworks, system libraries, X11 libraries, and libjack.
    unless s =~ /System|\/usr\/lib|\/usr\/X11|Jackmp|libjack|:$/ then
	#$stdout.print("Copying #{s}\n")
        `cp #{s} #{odir}/#{libdir}/`
    end
end

# now do it again
result =  `otool -L #{odir}/#{libdir}/*.dylib`
results = result.split("\n")
results.uniq!
results.each do |s|
    s = s.split[0]
    # exclude frameworks, system libraries, X11 libraries, and libjack.
    unless s =~ /System|\/usr\/lib|\/usr\/X11|Jackmp|libjack|:$/ then
      sbase = File.basename(s)
      targfile = "#{odir}/#{libdir}/#{sbase}"
      #$stdout.print("Targ is : " + targfile + "\n")
      if not File.exist?(targfile) then
	#$stdout.print("2nd stage Copying #{s}\n")
        `cp #{s} #{odir}/#{libdir}/`
      end
    end
end


Dir.chdir(odir)

# copy ardour binary to bindir/ardour


if File.exist?("../../gtk2_ardour/ardour-#{version}") then
   $stdout.print("Copying bin to #{bindir} ...\n");
   `cp ../../gtk2_ardour/ardour-#{version} #{bindir}/ardour`
end

`cp ../../libs/surfaces/*/*.dylib #{libdir}/surfaces`
# remove the basenames from libdir that are in surfaces (copied earlier)
`rm -f #{libdir}/surfaces/libardour_cp.dylib`
begin
  Dir.foreach(libdir+"/surfaces") {|x| unless ( x[0] == 46 or x.include?("libardour_cp")) then File.delete(libdir + "/" +x) end}
rescue
end

# vamp plugins
`cp ../../libs/vamp-plugins/*.dylib #{libdir}/vamp-plugins`


# copy gtk and pango lib stuff
`cp -R /opt/local/lib/pango #{libdir}/`
`cp -R /opt/local/lib/gtk-2.0 #{libdir}/`

# use our clearlooks
`rm -f #{libdir}/gtk-2.0/2.*/engines/libclearlooks.*`
# must use .so for it to be found :/
`cp ../../libs/clearlooks/libclearlooks.dylib #{libdir}/gtk-2.0/2.10.0/engines/libclearlooks.so`


def lipo_platforms_recurse(src1, src2, target)

  if not File.stat(src1).directory? then
    # normal file, lets lipo them if it doesn't already exist there
    isbin = `file #{src1}`.include?("Mach-O")
    if (! File.exist?(target)) and isbin then
      if File.exist?(src2) then
        $stdout.print("Lipo'ing " + target + "\n")
        `lipo -create -output #{target} #{src1} #{src2}`
      else
        # just copy it
        $stdout.print("Copying " + src1 + "\n")
        `cp #{src1} #{target}`
      end
    else
      #$stdout.print("Skipping " + target + "\n")
    end
  else
    # directory, recurse if necessary
    if File.exist?(src2) then
      # other dir exists, recurse
      
      # make targetdir if necessary
      if not File.exist?(target) then
        Dir.mkdir(target)
      end
      
      Dir.foreach(src1) do |file| 
        if file[0] != 46 then
          src1file = src1 + '/' + file
          src2file = src2 + '/' + file
          targfile = target + '/' + file
          lipo_platforms_recurse(src1file, src2file, targfile)
        end
      end
    else
      # just copy it recursively to target
      $stdout.print("Copying dir " + src1 + "\n")
      `cp -R #{src1} #{target}`
    end
  end
end

# lipo stuff together if both platforms libs and bins are here

if File.exist?(ppc_libdir) and File.exist?(i386_libdir) then
  $stdout.print("\nBoth platforms in place, lipo'ing...\n");
  `rm -rf lib/*`
  `rm -f bin/ardour`
  lipo_platforms_recurse(ppc_libdir, i386_libdir, "lib")
  lipo_platforms_recurse(i386_libdir, ppc_libdir, "lib")
  lipo_platforms_recurse(i386_bindir+'/ardour', ppc_bindir+'/ardour', "bin/ardour")

  # remove existing Ardour3.app
  `rm -rf Ardour3.app`

  $stdout.print("\nRunning Playtpus to create Ardour3.app  ...\n");

  `/usr/local/bin/platypus -D -X 'ardour' -a 'Ardour3' -t 'shell' -o 'None' -u 'Paul Davis' -i '/bin/sh' -V "#{version}" -s 'ArDr' -I 'org.ardour.Ardour3' -f 'bin' -f 'lib' -i 'Ardour3.icns' -f 'MenuBar.nib' -f 'ProgressWindow.nib' -f 'init' -f 'openDoc' 'script' 'Ardour3.app'`

  $stdout.print("\nCopying other stuff to Ardour3.app  ...\n");

  if not File.exist?("Ardour3.app/Contents/Resources/etc") then
    Dir.mkdir "Ardour3.app/Contents/Resources/etc" 
  end

  if not File.exist?("Ardour3.app/Contents/Resources/etc/ardour3") then
    Dir.mkdir "Ardour3.app/Contents/Resources/etc/ardour3" 
  end
  `cp ../../gtk2_ardour/ardour.bindings ../../gtk2_ardour/ardour.colors ../../gtk2_ardour/ardour.menus Ardour3.app/Contents/Resources/etc/ardour3/`
  `cp ../../ardour.rc ../../ardour_system.rc Ardour3.app/Contents/Resources/etc/ardour3/`
  `cp ardour3_mac_ui.rc Ardour3.app/Contents/Resources/etc/ardour3/ardour3_ui.rc`

  # copy other etc stuff
  if not File.exist?("Ardour3.app/Contents/Resources/etc/gtk-2.0") then
    `cp -R etc/gtk-2.0 Ardour3.app/Contents/Resources/etc/`
  end
  if not File.exist?("Ardour3.app/Contents/Resources/etc/pango") then
    `cp -R etc/pango Ardour3.app/Contents/Resources/etc/`
  end
  if not File.exist?("Ardour3.app/Contents/Resources/etc/fonts") then
    `cp -R /opt/local/etc/fonts Ardour3.app/Contents/Resources/etc/`
  end

  if not File.exist?("Ardour3.app/Contents/Resources/etc/profile.d") then
    `cp -R etc/profile.d Ardour3.app/Contents/Resources/etc/`
  end

  # share stuff

  if not File.exist?("Ardour3.app/Contents/Resources/share") then
    Dir.mkdir "Ardour3.app/Contents/Resources/share"
  end

  if not File.exist?("Ardour3.app/Contents/Resources/share/ardour3") then
    Dir.mkdir "Ardour3.app/Contents/Resources/share/ardour3"
    Dir.mkdir "Ardour3.app/Contents/Resources/share/ardour3/templates"
    `cp -R  ../../gtk2_ardour/icons ../../gtk2_ardour/pixmaps ../../gtk2_ardour/splash.png Ardour3.app/Contents/Resources/share/ardour3/`
    `cp ../../templates/*.template Ardour3.app/Contents/Resources/share/ardour3/templates/` 
  end

  # go through and recursively remove any .svn dirs in the bundle
  svndirs = `find Ardour3.app -name .svn -type dir`.split("\n")
  svndirs.each do |svndir|
    `rm -rf #{svndir}`
  end

  # make DMG
  `rm -rf macdist`
  Dir.mkdir("macdist")
  `cp -r README.rtf COPYING Ardour3.app macdist/`
  dmgname = "Ardour-#{version}"
  `rm -f #{dmgname}.dmg`
  $stdout.print("\nCreating DMG\n")
  `hdiutil create -fs HFS+ -volname #{dmgname} -srcfolder macdist #{dmgname}.dmg`


  $stdout.print("\nDone\n")

else
  # zip up libdir and bindir
  zipfile = "binlib_"+`uname -p`.strip() + ".zip" 
  $stdout.print("Zipping up #{libdir} and #{bindir} into #{zipfile}...\n")
  `zip -rq #{zipfile} #{libdir} #{bindir}`
  $stdout.print("Copy #{zipfile} to other platform's osx_packaging dir and run app_build.rb\nthere to complete universal build.\n")


end


