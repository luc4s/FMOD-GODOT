.. _doc_compiling_from_source:

Compiling from Sources
===============================

Setup
-----

FMOD is a commercial library so you will need to download the FMOD headers and libraries yourself. Libraries should be placed in 'libs/<platform>/<architecture>'.Headers should be placed in 'headers/' The fmod_installer.py script is also able to automatically install the headers for some platforms

.. code-block::
   # use --help for more options
   python3 fmod_installer.py setup <Fmod Version Number>

Build
-----

Use export.py to build the extension and add it to a project. Which manages copying over the necessary fmod dependencies

.. code-block::
   # use --help for more options
   python3 export.py build -p <platform>
   python3 export.py export <path/to/project>/addons

Alternatively refer to `introduction to the buildsystem`_

Web
---

Building for the web needs `Emscripten`_ 4.0.11, the version used by Godot 4.6's export templates. A different version
may build fine but fail to load in the browser.

.. code-block::

   # install and activate Emscripten (see the Emscripten docs)
   ./emsdk install 4.0.11
   ./emsdk activate 4.0.11
   source ./emsdk_env.sh

   # download the FMOD HTML5 libraries into libs/web/wasm32
   python3 fmod_installer.py -t web setup <Fmod Version Number>

   # builds template_debug and template_release, single-threaded
   python3 export.py build -p web

Web builds are always single-threaded (``threads=no``), and there is no editor build for the web. To build with SCons
directly, use ``scons platform=web target=template_debug api_version=4.6 build_profile=build_profile.json``.

FMOD is linked statically into the web library, so the FMOD libraries don't need to be copied into the addon. A project
using the addon still needs the desktop editor build to work in the editor. See :ref:`doc_exporting_for_web`.

.. _Emscripten: https://emscripten.org/docs/getting_started/downloads.html

.. _introduction to the buildsystem: https://docs.godotengine.org/en/stable/engine_details/development/compiling/introduction_to_the_buildsystem.html
