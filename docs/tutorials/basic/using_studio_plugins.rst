.. _doc_using_studio_plugins:

Using Studio Plugins
====================

FMOD plugins are shared libraries that add DSP effects, instruments or codecs, for example the Resonance Audio
plugin that ships with the FMOD Engine. For general information refer to `fmod plugin docs`_.

Each platform needs its own build of a plugin. Copy the libraries you need into your project, for example:

.. code-block:: none

    res://fmod_plugins/linux/libresonanceaudio.so
    res://fmod_plugins/windows/resonanceaudio.dll
    res://fmod_plugins/macos/libresonanceaudio.dylib
    res://fmod_plugins/android/arm64-v8a/libresonanceaudio.so
    res://fmod_plugins/android/x86_64/libresonanceaudio.so

Configuration
-------------
Project Settings
^^^^^^^^^^^^^^^^
*fmod/plugins/plugins*: Dictionary from plugin library to priority. Pick the library with the file dialog, or enter
an absolute path or a path relative to *fmod/plugins/path*. Use `feature overrides`_ to list the libraries of each
platform, for example *fmod/plugins/plugins.linux* and *fmod/plugins/plugins.windows*.

*fmod/plugins/path*: Optional folder that relative paths in *fmod/plugins/plugins* are resolved against. If it is
empty, they are relative to the project folder (``res://``).

.. image:: img/project_settings.png

Plugins are loaded when FMOD starts, so after changing these settings restart the editor, or use *Restart Server* in
the menu of the FMOD event browser. If a plugin can't be loaded, an error is printed. When the editor starts, this
happens before the Output panel exists, so the error only appears in the terminal that started the editor; use
*Restart Server* to see it in the Output panel.

Exporting
---------
When you export the project, the plugin libraries for the target platform are copied to where the platform keeps
native libraries, and the game loads them from there:

- **Windows and Linux:** next to the executable.
- **macOS:** in ``Contents/Frameworks`` inside the app bundle.
- **Android:** in ``lib/<abi>`` inside the APK. Put each library in a folder named after its ABI (``arm64-v8a``,
  ``armeabi-v7a``, ``x86_64`` or ``x86``), as in FMOD's Android SDK, and list all of them in
  *fmod/plugins/plugins.android*. Libraries for ABIs that the export preset doesn't include are left out. File names
  must start with ``lib``.

A library that can't be found is reported in the export log and left out of the export.

.. note::

    Plugins are not supported on the Web platform. See :ref:`doc_exporting_for_web`.

.. _feature overrides : https://docs.godotengine.org/en/stable/tutorials/export/feature_tags.html#overriding-project-settings

.. _fmod plugin docs: https://www.fmod.com/docs/2.03/api/dsp-plugin-api-guide.html
