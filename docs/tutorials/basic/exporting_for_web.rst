.. _doc_exporting_for_web:

Exporting for the Web
=====================

FMOD-GODOT supports the Web platform (HTML5) with Godot's official web export templates. Read
`Exporting for the Web`_ in the Godot documentation first; this page only covers what is specific to FMOD.

Requirements
------------

- Godot 4.6 or newer, with the official export templates.
- The FMOD-GODOT addon built for web (``bin/web/wasm32/libFmodGodot.template_*.nothreads.wasm``). FMOD is
  statically linked into it, so no other FMOD files need to be shipped.

Export preset
-------------

In the Web export preset:

- Enable **Variant > Extensions Support**. Without it, the extension (and FMOD) is not loaded.
- Disable **Variant > Thread Support**. FMOD for the web is only built without threads. A preset with thread support
  enabled does not find a matching library.

Without thread support, no special server headers (COOP/COEP) are needed.

Audio starts after the first user input
---------------------------------------

Browsers don't allow a page to play audio until the user interacts with it. FMOD is initialized when the game starts,
but stays silent until the first click, tap or key press, after which audio starts automatically. Events started
before that are not lost: they play (from their current position) once audio is unlocked.

If your game starts with music, consider a "click to start" screen. Gamepad input does not count as a user
interaction for browsers.

:ref:`FmodAudioServer.mixer_suspend()<class_FmodAudioServer_method_mixer_suspend>` and
:ref:`FmodAudioServer.mixer_resume()<class_FmodAudioServer_method_mixer_resume>` can be used to suspend audio, for
example while the game is paused.

Limitations
-----------

These come from FMOD's HTML5 build or from the web platform:

- **No Live Update or profiling.** Browsers don't provide the network sockets FMOD Studio needs.
  *fmod/general/live_update* is ignored on the web.
- **No FMOD plugins.** *fmod/plugins/plugins* and *fmod/plugins/path* are ignored on the web, with a warning.
- **Single-threaded.** FMOD updates once per frame on the main thread, after all ``_process()`` calls. Loading sample
  data (for example with *preload_sample_data*) also happens there, so loading large banks can cause frame hitches.
  If you wait for sample data to load, check once per frame (``await get_tree().process_frame``) instead of in a loop.
- **Reduced FMOD core in release exports.** Release exports use FMOD's smaller "reduced" library. Events from banks
  work as usual, but the Core API cannot load audio files such as WAV, OGG or MP3 directly. Debug exports use the
  full library.
- **Banks are loaded into memory.** On the web, the whole exported ``.pck`` is downloaded and kept in memory before
  the game starts. Keep this in mind for large banks.
- **GDScript only.** Godot does not support exporting C# projects to the web.
- ``StudioEventDescription`` callbacks are not supported by FMOD on the web. Use event instance callbacks instead.

FmodEventAudioStream
--------------------

Godot plays streams on the web as "samples" by default (*audio/general/default_playback_type.web*), which
:ref:`FmodEventAudioStream<class_FmodEventAudioStream>` doesn't support. Set **Playback Type** to **Stream** on the
``AudioStreamPlayer`` nodes that use it, or set the *audio/general/default_playback_type.web* project setting to
**Stream**.

Project settings
----------------

Any setting can be changed for the web only with a ``.web`` `feature override`_. The plugin already defines two:

- *fmod/general/dsp/buffer_length.web* = 2048
- *fmod/general/dsp/buffer_count.web* = 2

FMOD recommends these on HTML5, because some devices stutter with the default of 4 buffers of 1024 samples. Because
these overrides exist, changing *fmod/general/dsp/buffer_length* or *buffer_count* has no effect on the web: change
the ``.web`` settings instead.

Banks
-----

Banks are exported like on other platforms. For :ref:`doc_multiplatform_builds`, the feature tag of the web platform is
``web``.

Debugging
---------

Messages from FMOD and Godot appear in the browser's developer console. Raising *fmod/debug/logging_level* only has an
effect in debug exports.

.. _Exporting for the Web: https://docs.godotengine.org/en/stable/tutorials/export/exporting_for_web.html
.. _feature override: https://docs.godotengine.org/en/stable/tutorials/export/feature_tags.html#overriding-project-settings
