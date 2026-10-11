extends Node

# Adds the probe at runtime so the scene still loads if the extension failed to load.
func _ready() -> void:
	print("FMODPROBE SCENE threads=", OS.has_feature("threads"), " web=", OS.has_feature("web"))
	if not ClassDB.class_exists("FmodWebProbe"):
		print("FMODPROBE ERROR extension not loaded")
		return
	var probe: Node = ClassDB.instantiate("FmodWebProbe")
	probe.name = "Probe"
	add_child(probe)
