# Scripts

This directory contains any miscellaneous batch files, Python scripts, or other scripts used to make certain maintenance tasks easier.

## build*.bat

These are scripts that will automatically rebuild the required libraries from source.  Make sure you initialize the submodules with `git submodule update --init --recursive` first!

The build scripts are for the people who do _WANT_ to start from scratch, whether it's for security, validation, or updating to a new version.

NOTE: You do _NOT_ need to rebuild libraries from scratch.  The pre-built libraries are already included in the `src\` directories.

NOTE: Re-built libraries do not match 1:1 with previous builds.  The compiler puts other information into the binaries so they won't ever match 100%

## keymap_*py

These Python scripts are for maintenance of the `KeyMap.txt` in an automated fashion.

* `keymap_generate_symbols_and_readme.py` - regenerates the defaults and symbols directly from source.
* `keymap_convert_readme_xp_compat.py` - the `KeyMap-README.html` files use a CSS2 feature to separate the translation from the built-in table, this takes existing files and converts them to be XP-compatible (for XP releases ONLY!)

## strings_txt_*.py

These Python scripts are for translation and localization maintenance.

* `strings_txt_builder.py` - regenerate the reference `strings.txt` from source
* `strings_txt_sync_all.py` - sync all `strings_XX.txt` with the reference, automatically sorts entries and makes it easier to diff with the reference
