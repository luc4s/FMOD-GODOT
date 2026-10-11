extends Node
# Phase 0 web spike driver. Emits "SPIKE <json>" lines that tools/browser_test.mjs parses.

const BANK_DIR := "res://banks"
const EVENT_PATH := ""  # empty: play the first event found in the loaded banks
# Headless browsers auto-allow audio, so the gesture unlock is emulated: suspend the mixer after
# starting playback and resume it on the first input, as the plugin would on web.
const SUSPEND_UNTIL_INPUT := true

var spike: Object
var ok := false
var elapsed := 0.0


func report(step: String, error: String) -> void:
	print("SPIKE ", JSON.stringify({"step": step, "error": error}))


func _ready() -> void:
	if not ClassDB.class_exists("FmodWebSpike"):
		report("extension_loaded", "FmodWebSpike class missing: the GDExtension did not load")
		return
	report("extension_loaded", "")
	spike = ClassDB.instantiate("FmodWebSpike")
	var err: String = spike.init_fmod(0, 0)
	report("init", err)
	if err != "":
		return
	var banks := DirAccess.get_files_at(BANK_DIR)
	if banks.is_empty():
		report("load_bank", "no banks in " + BANK_DIR)
		return
	# Strings and master banks first so later banks can resolve paths.
	var ordered := Array(banks)
	ordered.sort_custom(func(a, b): return a.begins_with("Master") and not b.begins_with("Master"))
	for bank in ordered:
		if bank.ends_with(".bank"):
			report("load_bank:" + bank, spike.load_bank(BANK_DIR.path_join(bank)))
	report("play", spike.play(EVENT_PATH))
	if SUSPEND_UNTIL_INPUT:
		report("mixer_suspend", spike.mixer_suspend())
	ok = true


func _process(delta: float) -> void:
	if not ok:
		return
	var err: String = spike.update()
	if err != "":
		report("update", err)
	elapsed += delta
	if elapsed >= 0.5:
		elapsed = 0.0
		print("SPIKE ", JSON.stringify({"step": "status", "status": spike.get_status()}))


func _input(event: InputEvent) -> void:
	if ok and (event is InputEventMouseButton or event is InputEventKey) and event.is_pressed():
		report("mixer_resume", spike.mixer_resume())
