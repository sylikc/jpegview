"""

Now that a reference strings.txt is created, it would be difficult to know which strings are used or not used... aka which translations need update?  which ones don't

This script takes an existing translation file, and formats it to match line by line with strings.txt

It will comment out translations which no longer exist, and leave empty the ones which are not translated.


Autosync'd to reference....

"""
import re
from pathlib import Path
#from datetime import datetime, timezone
import pprint

UTF8 = 'utf-8-sig'  # UTF-8 with BOM


CONFIG_DIR = Path(__file__).resolve().parent.parent.parent / r"src\JPEGView\Config"

REFERENCE_STRINGS = Path(__file__).resolve().parent.parent.parent / r"src\JPEGView\Config\strings.txt"


def parse_strings_txt(filepath):
    """
    returns a dict and a list...
        dict: key-value pairs for existing translations
        list: comments in the file
    """
    # first cache the file entirely, we need to build up the mappings
    with open(filepath, 'rt', encoding=UTF8) as f:
        file_lines = f.readlines()

    # loop the lines in the file to get the mapping
    # the mapping is "english string":
    file_dict = {}
    file_comments = []  # this is a list of comments which will be moved to the new file

    for line in file_lines:
        # strip the UTF-8 header if it exists
        #line = line.lstrip(chr(65279))
        line = line.rstrip("\r\n")

        if line.startswith("//"):
            # check a special condition
            if "\t" in line:
                # this is a previously commented out translation
                try:
                    (e, t) = line[2:].split("\t", 1)  # if this causes an error, we have to fix on a case-by-case basis...
                except ValueError:
                    t_pos = line.find("\t")
                    print(t_pos, line)
                    print(f"'{line[t_pos-5:t_pos+5]}'")
                    print(ord(line[0]))
                    raise

                if e in file_dict:
                    raise KeyError(f"duplicate translation string found in comments: {e}")
                else:
                    # there should be no spaces around it
                    file_dict[e.strip()] = t.strip()
            else:
                file_comments.append(line)  # note that this doesn't have the newline for writelines(), so we dump manually later

            continue

        if len(line) == 0:
            # ignored
            continue

        if "\t" not in line:
            #raise ValueError(f"no tab on this line?!: '{line}'")
            # assume it's just the english text... doesn't matter what it really is, as it will get captured in the "out of date" texts
            if line.strip() in file_dict:
                raise KeyError(f"already exists: {line.strip()}")
            file_dict[line.strip()] = ""
            continue

        #print(line)
        try:
            (e, t) = line.split("\t", 1)  # accommodate for multiple tabs in one line
        except ValueError:
            print(line)
            raise


        # english / translated
        e_strip = e.strip()
        t_strip = t.rstrip()

        if e_strip in file_dict:
            raise KeyError(f"already exists: {e_strip}")
        if e_strip == t_strip:
            # this is actually a blank translation, aka, not translated ... in legacy files this was the way it was done, but with new files, we want to make it clear when stuff isn't translated
            file_dict[e_strip] = ""
        else:
            file_dict[e_strip] = t_strip

    return (file_dict, file_comments)


def sync_strings_to_reference(reference_lines, filepath):
    """
    returns the LIST of missing, and DICT of what isn't translated yet

    originally we wanted to stick outdated translations as comments near the alphabetical order of the previous one for easier translation,
    but this is both a bit difficult, and pollutes the translation file, so it's better to write it out separately
    """

    (file_dict, file_comments) = parse_strings_txt(filepath)

    missing_list = []

    # it's safe to re-write the file because it's version controlled... if we need to revert, we can checkout a previous copy
    with open(filepath, 'wt', encoding=UTF8) as f:
        found_start_line = False
        finished_comments = False

        # loop the reference lines
        for line in reference_lines:
            # strip the UTF-8 header if it exists (don't need anymore)
            #line = line.lstrip(chr(65279))
            line = line.rstrip("\r\n")  # there SHOULD BE no spaces before or after, this is to get rid of the newline

            # ignore lines until UTF-8 (this condition can be adjusted as needed)
            if not found_start_line:
                if "UTF-8" in line:
                    found_start_line = True
                else:
                    # skip this line
                    continue

            # write the reference comments verbatim into file
            if line.startswith("//"):
                f.write(f"{line}\n")

                # remove this line from file_comments if it exists
                # so the header doesn't get re-written on subsequent updates
                try:
                    #pprint.pprint(file_comments)
                    #print(f"try remove: {line}")
                    file_comments.remove(line)
                    #print("ok")
                except ValueError:
                    pass

                # TODO, extra logic to keep from re-writing header

                continue

            # empty lines go over as well
            if len(line) == 0:
                f.write("\n")
                continue

            # if we got here, the initial comments block is complete, dump the comments section
            if not finished_comments:
                #f.writelines(file_comments)
                # no \r\n so do manually
                for x in file_comments:
                    # skip special sequences (so headers don't get rewritten)
                    if x.startswith("// :::"):
                        continue

                    f.write(f"{x}\n")
                finished_comments = True

            # what's remaining is the english string to split, there should be nothing on the other side though
            (e_index, e_none) = line.split("\t")
            if len(e_none) != 0:
                raise ValueError("Expected no value on reference")


            if e_index in file_dict:
                if len(file_dict[e_index].strip("\t")) != 0:  # accommodate for multi-tabs in a line
                    # translation found
                    f.write(f"{e_index}\t{file_dict[e_index]}\n")
                else:
                    # translation was actually not defined yet
                    # it's in the list but missing a translation
                    f.write(f"{e_index}\t{file_dict[e_index]}\n")  # write no-translation e_index, but with any tabs that might have been existing
                    missing_list.append(e_index)  # save to return separately

                # remove translation from the file_dict, because it's not an outdated translation
                del file_dict[e_index]
            else:
                # not found
                f.write(f"{e_index}\t\n")  # write no-translation e_index
                missing_list.append(e_index)  # save to return separately

        # dump out all remaining keys where there used to be translation but no more
        if len(file_dict) != 0:
            f.write(f"\n// ::: Out-of-date Translations ::: (Not in Use anymore, safe to delete) //\n")
        for k, v in file_dict.items():
            f.write(f"//{k}\t{v}\n")


    return (missing_list, file_dict)







if __name__ == "__main__":

    # only read the reference file once
    with open(REFERENCE_STRINGS, 'rt', encoding=UTF8) as f:
        ref_file = f.readlines()

    for p in CONFIG_DIR.glob("strings_*.txt"):
        if "missing" in str(p):
            continue

        print(p)
        (missing, outdated) = sync_strings_to_reference(ref_file, p)
        with open(p.with_suffix(".missing.txt"), 'wt', encoding=UTF8) as f:
            for x in missing:
                f.write(f"{x}\n")

    """
        reset files in git-bash

        $ find src/JPEGView/Config -name "strings_*.txt" -exec git checkout -- {} \;
    """

    #(missing, outdated) = sync_strings_to_reference(ref_file, CONFIG_DIR / "strings_fr_test.txt")
    #pprint.pprint(missing)
    #pprint.pprint(outdated)