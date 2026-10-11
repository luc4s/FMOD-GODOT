.. _doc_multiplatform_builds:

Multiplatform Builds
====================
.. note::

    Multiplatform builds are experimental and subject to change. A general purpose plugin like `Resource Remaps`_ might be more useful for your project.

A Multiple Platform Build lets each export use the banks that FMOD Studio built for its platform, for example
desktop banks on Windows, macOS and Linux and HTML5 banks on the web.

Setup
=====

In project settings:

* Set *fmod/banks/import/source_type* to 'Multiple Platform Build'.
* Set *fmod/banks/import/bank_directory* to the folder that contains one subfolder per platform, for example
  ``res://banks``. In FMOD Studio, build your banks into this folder.
* Set *fmod/banks/import/base_platform_folder* to the subfolder that your project uses in the editor. The default is
  ``pc``.

Every direct subfolder of the bank directory is a *platform folder*:

* The **base platform folder** holds the banks that the editor, *Load All*, *Specified* banks and your scenes and
  scripts use. Always reference banks by their path in this folder, for example ``res://banks/pc/Music.bank``. Its name
  does not have to be a feature tag, so FMOD Studio's default ``Desktop`` works too.
* Every other platform folder is named after one or more `feature tags`_ separated by ``.``, for example ``web``,
  ``mobile``, ``linux`` or ``web.nothreads``. A folder *matches* an export when the export has all of its tags. Tags
  are case-sensitive, so FMOD Studio's ``HTML5`` must be renamed ``web``.

Example:

.. code-block:: none

    res://banks/
        pc/            base platform folder: Master.bank, Master.strings.bank, Music.bank
        web/           Master.bank, Music.bank, WebOnly.bank
        linux/         Music.bank

What an export contains
=======================

When you export, each bank of the base platform folder is replaced by the bank with the same name (and the same
subfolder) from the most specific matching platform folder, the one with the most tags. If no matching folder has
that bank, the base bank is exported unchanged. The bank is stored once, at its path in the base platform folder, so
your project loads the right bank without any change.

With the example above:

* **Linux**: ``pc/Music.bank`` contains ``linux/Music.bank``. The other banks come from ``pc``.
* **Windows** and **macOS**: no folder matches, so the ``pc`` banks are exported unchanged.
* **Web**: ``pc/Master.bank`` and ``pc/Music.bank`` contain the ``web`` banks. ``web/WebOnly.bank`` has no base
  version, so it is exported at its own path.

Other rules:

* Platform folders that do not match the export are not exported.
* A bank that only exists in a matching platform folder is exported at its own path. Load it yourself, for example
  ``if OS.has_feature("web"): load("res://banks/web/WebOnly.bank")``. *Load All* does not load it.
* If two matching folders with the same number of tags have the same bank, the first one in alphabetical order is
  used. Rename one of them, for example ``web.nothreads``, to choose.
* Banks directly in the bank directory, or outside it, are exported unchanged.
* If the bank directory or the base platform folder does not exist, all banks are exported unchanged and the export
  shows a warning.

Loading banks
=============

With *fmod/banks/initialization/load_banks* set to *All*, FMOD Godot loads every bank directly in the base platform
folder (subfolders are not loaded). In the editor this is the base folder itself. In an export it holds the banks
chosen for that platform.

.. _Resource Remaps: https://store.godotengine.org/asset/allenwp/resource-remaps/
.. _feature tags: https://docs.godotengine.org/en/stable/tutorials/export/feature_tags.html
