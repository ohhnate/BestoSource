#!/bin/sh
scons p=linux target=editor module_gdscript_enabled=no module_mono_enabled=yes
bin/godot.linuxbsd.editor.x86_64.mono --generate-mono-glue modules/mono/glue
python ./modules/mono/build_scripts/build_assemblies.py --godot-output-dir=./bin --godot-platform=linux
