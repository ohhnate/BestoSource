@echo off
scons p=windows target=editor module_gdscript_enabled=no module_mono_enabled=yes && bin\godot.windows.editor.x86_64.mono --generate-mono-glue modules/mono/glue &&  python ./modules/mono/build_scripts/build_assemblies.py --godot-output-dir=./bin --godot-platform=windows
