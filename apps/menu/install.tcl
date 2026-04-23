if {[file exists "menu"] == 1} {
    if {[file exists "menu.old"] == 1} {
	file delete -force menu.old
    }
    file rename menu/ menu.old/
}
# Set SDK workspace
setws menu

app create -name menu -hw hardware/pacman.xsa -os standalone -proc ps7_cortexa9_0 -lang C -template {Hello World}

# rename hello world app to hardware check:
file rename menu/menu/src/helloworld.c menu/menu/src/menu.c

file copy apps/menu/run.tcl menu/
file copy apps/menu/build.tcl menu/

# now copy everything from src/, -force overwrites menu.c with the real one
foreach f [glob apps/menu/src/*] {
    file copy -force $f menu/menu/src/
}

# Run the application:
cd menu

app build menu

#source run.tcl

#cd ..
