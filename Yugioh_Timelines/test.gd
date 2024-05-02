#scene script
extends Node

var scene_array = global.global_array

func _ready():
	global.global_array = [9,9,9,9,9,9]
	print(global.global_array)
	print(scene_array)
