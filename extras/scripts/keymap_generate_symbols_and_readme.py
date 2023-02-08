"""

This autogenerates the "KeyMap_internal.config"

The file is meant to be read by JPEGView on startup, and basically defines the string->number mapping, so that
CMainDlg can be called with a specific number to execute a command.  It uses the same names as the internal code
in resource.h.

The KeyMap_internal.config is not meant to be viewed/modified by the user.  It does not require translations.


(this script wasn't written with efficiency in mind, there's things like compiling the regex, or like using better
data structures... but I just went quick and dirty)

"""
import re
from pathlib import Path
from datetime import datetime, timezone

SRC_PATH = Path(__file__).resolve().parent.parent.parent / "src"


def keymap_resource_parse():
    """ returns 2 dicts, one with IDM_=int, and the other with IDM_=string(comment) """

    RESOURCE_FILE = SRC_PATH / r"JPEGView\resource.h"

    d_int = {}
    d_comment = {}

    with open(RESOURCE_FILE, 'r') as f:
        while True:
            line = f.readline()
            if not line:
                break

            # remove the newlines captured by readline()
            line = line.strip()

            m = re.fullmatch("(#define)\s+(\w+)\s+(\d+)\s+// :KeyMap: (.*)", line)
            if m:
                d_int[m.group(2)] = int(m.group(3))
                d_comment[m.group(2)] = m.group(4)

    return (d_int, d_comment)


def keymap_generate_internal_config():
    """ this will generate the internal config file

        it is intentionally made to be less readable by not doing any padding
    """
    TARGET_FILE = SRC_PATH / r"JPEGView\Config\symbols.km"  # make filename discreet, since it really shouldn't confuse the user

    (d_index, d_comment) = keymap_resource_parse()

    # no timestamp needs to be with this file, as it can be regenerated from source at any time
    with open(TARGET_FILE, 'wt', encoding='utf-8') as f:
        f.write("""// === Internal KeyMap Symbols Configuration File ===
//
// !!! THIS FILE WAS AUTO-GENERATED and can differ between releases.  DO NOT MODIFY !!!
//
// For the curious: JPEGView reads this file on startup to define the internally mapped values to KeyMap definitions.
//
// NOTE: This file does NOT need to be localized, as users do not (should not) interact with this file.
//
// Devs: see "extras\scripts" for more info.\n\n""")

        for k, v in d_index.items():
            f.write(f"#define {k} {v}\n")






def keymap_generate_reference_readme():
    """ this will generate the reference readme for what to do with the KeyMap.txt

    the generated file is NOT PARSED by JPEGView at all, it's only a readme, so it can be translated without worry of a bad edit
    """
    TARGET_FILE = SRC_PATH / r"JPEGView\Config\KeyMap-README.html"

    (d_index, d_comment) = keymap_resource_parse()

    # get the longest string in the index
    max_len = 0

    for x in d_index:
        if len(x) > max_len:
            max_len = len(x)

    # pad out the string to string + 4
    padding = max_len + 4

    # UPDATE THE LASTUPDATED if this text gets updated so we know when translations are out of date!
    with open(TARGET_FILE, 'wt', encoding='utf-8') as f:
        f.write(f"""<html>
<head>
    <title>
        JPEGView - KeyMap.txt README
    </title>
    <style>""")


        for k, v in d_comment.items():
            v_escaped = v.replace('\\', '\\\\').replace(r'"', r'\"')  # CSS needs certain things escaped
            # https://stackoverflow.com/questions/2741312/using-css-to-insert-text
            # CSS3 is different: https://developer.mozilla.org/en-US/docs/Learn/CSS/Howto/Generated_content
            f.write(f"""
.{k}:before {{
  content: "{v_escaped}"
}}
""")


        f.write("</style></head>")

        f.write(f"""<body>

<header>
    <h1>
        KeyMap Configuration Readme
    </h1>
</header>

JPEGView allows customizing what each key and key combination will do.

<h2>
    Translator Credits
</h2>
Language: English<br />
Translator Name(s): <br />
Contact(s): <br />
Last Updated: 31-JAN-2023<br />

<h1>
    Basics
</h1>
<p>
    Edit "KeyMap.txt" to change keys and actions to your preference.
</p>

<p>
    The format is:
    <pre>
      &lt;Key or mouse combination&gt;			&lt;command&gt;
    </pre>
</p>

<b>
    NOTE: A small number of keys is predefined and cannot be changed:
</b>
<ul>
    <li>F1 for help</li>
    <li>Alt+F4 for exit</li>
    <li>0..9 for slide show</li>
</ul>

<h1>
    Keys
</h1>

The following keys are recognized:
<ul>
    <li>Alt, Ctrl, Shift</li>
    <li>Esc, Return, Space, End, Home, Back, Tab, PgDn, PgUp</li>
    <li>Left, Right, Up, Down, Insert, Del, Plus, Minus, Mul, Div, Comma, Period</li>
    <li>A .. Z</li>
    <li>F1 .. F12</li>
</ul>

<h2>
    Mouse
</h2>

<p>
    It is also possible to bind a command to a mouse button.<br />
    Be careful when you override the default functionality (e.g. to pan with left mouse)!
</p>

Mouse buttons can be combined with:  <b>Alt, Ctrl, Shift</b>
but not with other keys or other mouse buttons.

<p>
    The following mouse buttons are recognized:
    <ul>
        <li>MouseL, MouseR, MouseM</li>
        <li>MouseX1, MouseX2</li>
        <li>MouseDblClk</li>
    </ul>

    <i>
        (Left, right, middle, extra mouse buttons, mouse left double click)
    </i>
</p>


<h2>
    Rules
</h2>

<p>
    Mapping multiple keys to the same command is allowed, but not multiple commands to the same key
</p>

<p><b>
    WARNING: Be sure not to corrupt the "KeyMap.txt", or else no keys will work anymore!
</b></p>


<h1>
    Supported Commands
</h1>

<p>
    The following is a list of available JPEGView commands which can be mapped to keys/mouse
    On the left is the command, on the right is a brief description of the command.
</p>

<p>
    <b>
        NOTE: THIS LIST WAS AUTO-GENERATED
    </b>
    <br />
    <i>
        -- Last Updated: {datetime.now(timezone.utc).isoformat()}
    </i>
</p>

<!--
<p><i>
    (Devs: see "extras\scripts" for more info.)
</i></p>
-->

<!-- ###################### TRANSLATORS - THERE IS _NOTHING_ TO TRANSLATE BELOW THIS LINE ###################### -->
\n\n""")


        #f.write(f"{'***** Command ***** ':<{padding}} ***** Description *****\n")
        f.write(f"""
<table border="1">
  <tr>
    <th>
      Command
    </th>
    <th>
      Description
    </th>
  </tr>\n\n""")
        for k, v in d_comment.items():
            # pad with dots so that there's no question of whether it's spaces, tabs, etc... and things line up... also looks prettier than _ or -
            # https://www.peterbe.com/plog/how-to-pad-fill-string-by-variable-python
            #f.write(f"{k + ' ':.<{padding}} {v}\n")  # write to text file style

            f.write(f"""
  <tr><td>{k}</td>
    <td>
      <span class="{k}" />
    </td>
  </tr>\n""")

        f.write("</html>")




if __name__ == "__main__":
    #(d_index, d_comment) = keymap_resource_parse()
    #print(d_index)

    # generate the internal from source
    keymap_generate_internal_config()

    # generate the reference file (aka README)
    keymap_generate_reference_readme()






# this is what it looked like before I went to HTML
f"""=== KeyMap Configuration Readme ===

JPEGView allows customizing what each key and key combination will do.

- Translation Credits -
Language=English
TranslatorName=
Contact=
LastUpdated=31-JAN-2023

----- Basics -----
Edit "KeyMap.txt" to change keys and actions to your preference.

The format is:
  <Key or mouse combination>			<command>

NOTE: A small number of keys is predefined and cannot be changed:
* F1 for help
* Alt+F4 for exit
* 0..9 for slide show


----- Keys -----
The following keys are recognized:
* Alt, Ctrl, Shift
* Esc, Return, Space, End, Home, Back, Tab, PgDn, PgUp
* Left, Right, Up, Down, Insert, Del, Plus, Minus, Mul, Div, Comma, Period
* A .. Z
* F1 .. F12


----- Mouse -----
It is also possible to bind a command to a mouse button.
Be careful when you override the default functionality (e.g. to pan with left mouse)!
Mouse buttons can be combined with:  Alt, Ctrl and Shift
but not with other keys or other mouse buttons.

The following mouse buttons are recognized:
* MouseL, MouseR, MouseM
* MouseX1, MouseX2
* MouseDblClk
(Left, right, middle, extra mouse buttons, mouse left double click)


----- Rules -----
NOTE: Mapping multiple keys to the same command is allowed, but not multiple commands to the same key

WARNING: Be sure not to corrupt the "KeyMap.txt", or else no keys will work anymore!


----- Supported Commands -----
The following is a list of available JPEGView commands which can be mapped to keys/mouse
On the left is the command, on the right is a brief description of the command.

NOTE: THIS LIST WAS AUTO-GENERATED FROM SOURCE -- LastUpdated: {datetime.now(timezone.utc).isoformat()}
(JPEGView does not read this file, it's for documentation only)
(Devs: see "extras\scripts" for more info.)\n\n\n"""