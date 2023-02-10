"""
Windows XP uses IE 6.x so it isn't compatible with the CSS2's use of :before

Thus, this script is to take a given README and convert it to the old-style so WinXP shows the same text

It takes one that is already done, because the ones which use CSS2 are much easier to translate
"""
import re
from pathlib import Path
import sys

from util_common import get_all_text_between

def keymap_readme_xp_compat(filepath):
    """

    search for all ".<span>:before.*{.*content: " .... "} between <style> blocks

    replace all <span class="..." /> with the content
    """

    if not filepath.exists():
        raise FileNotFoundError


    with open(filepath, 'r') as f:
        file_contents = f.read()

        style_block = get_all_text_between(None, "<style>", "</style>", search_str=file_contents)
        #print(style_block)

    # generate a list of the content strings
    content = {}

    # https://www.dataquest.io/blog/regex-cheatsheet/
    for idm_var, c in re.findall('\.(\w+):before\s*\{\s*content\s*:\s*"(.*?)"\s*\}', style_block):
        #print(idm_var, c)
        content[idm_var] = c
        print(f"style {idm_var}={c}")


    # replace the <spans>
    for span in re.findall('(<span\s+class="\w+"\s*/>)', file_contents):
        m = re.search('"(\w+)"', span)
        if not m:
            raise RuntimeError("shouldn't happen")

        #print(span, content[m.group(1)])
        file_contents = file_contents.replace(span, content[m.group(1)])

    #print(file_contents)

    # dump it back out
    with open(filepath.with_suffix(".xp.html"), 'wt', encoding='utf-8') as f:
        f.write(file_contents)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise Exception("Need arg for file")

    keymap_readme_xp_compat(Path(sys.argv[1]))
