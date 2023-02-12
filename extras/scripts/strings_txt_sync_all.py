"""

Now that a reference strings.txt is created, it would be difficult to know which strings are used or not used... aka which translations need update?  which ones don't

This script takes an existing translation file, and formats it to match line by line with strings.txt

It will comment out translations which no longer exist, and leave empty the ones which are not translated.


Autosync'd to reference....

"""
import re
from pathlib import Path
from datetime import datetime, timezone
import pprint
import sys

# https://stackoverflow.com/questions/8898294/convert-utf-8-with-bom-to-utf-8-with-no-bom-in-python
UTF8 = 'utf-8-sig'  # UTF-8 with BOM


CONFIG_DIR = Path(__file__).resolve().parent.parent.parent / r"src\JPEGView\Config"

REFERENCE_STRINGS = Path(__file__).resolve().parent.parent.parent / r"src\JPEGView\Config\strings.txt"


def parse_strings_txt(filepath):
    """
    returns a dict and a list and a dict...
        dict: key-value pairs for existing translations
        list: comments in the file
        dict: special dict, with translation info
    """
    # first cache the file entirely, we need to build up the mappings
    with open(filepath, 'rt', encoding=UTF8) as f:
        file_lines = f.readlines()

    # loop the lines in the file to get the mapping
    # the mapping is "english string":
    file_dict = {}
    file_comments = []  # this is a list of comments which will be moved to the new file
    file_info = {}

    for line in file_lines:
        # strip the UTF-8 header if it exists
        #line = line.lstrip(chr(65279))
        line = line.rstrip("\r\n")

        if line.startswith("//"):
            # check a special condition
            if line.startswith("//: "):
                # check this comment for special sequences, like this translator sequence
                (k, v) = line[4:].split(":", 1)
                file_info[k] = v.strip()
                print(f"FOUND INFO: {k}={v.strip()}")
            elif "\t" in line:
                # this is a previously commented out translation
                try:
                    (e, t) = line[2:].split("\t", 1)  # if this causes an error, we have to fix on a case-by-case basis...
                except ValueError:
                    t_pos = line.find("\t")
                    print(t_pos, line)
                    print(f"'{line[t_pos-5:t_pos+5]}'")
                    print(ord(line[0]))
                    raise

                e_strip = e.strip()
                t_strip = t.strip()
                if e in file_dict:
                    print(f"duplicate translation string found in comments: {e}")
                    if file_dict[e_strip] != t_strip:
                        raise KeyError("and values don't match")  # this is an error
                else:
                    # there should be no spaces around it
                    file_dict[e_strip] = t_strip
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

        #if e_strip == t_strip:
        #    # this is actually a blank translation, aka, not translated ... in legacy files this was the way it was done, but with new files, we want to make it clear when stuff isn't translated
        #    file_dict[e_strip] = ""
        # in the past, the reference was something=something to show it wasn't translated... because the reference has changed, and the fact there is legitimately no translations for certain strings,
        # after a few runs to clean out the old way, we need to allow the fact that they are equal

        file_dict[e_strip] = t_strip

    return (file_dict, file_comments, file_info)


def sync_strings_to_reference(reference_lines, filepath):
    """
    returns the LIST of missing, and DICT of what isn't translated yet

    originally we wanted to stick outdated translations as comments near the alphabetical order of the previous one for easier translation,
    but this is both a bit difficult, and pollutes the translation file, so it's better to write it out separately
    """

    (file_dict, file_comments, info_dict) = parse_strings_txt(filepath)

    missing_list = []
    count_reference = 0  # number of strings in reference

    # it's safe to re-write the file because it's version controlled... if we need to revert, we can checkout a previous copy
    with open(filepath, 'wt', encoding=UTF8) as f:
        found_start_line = False
        info_block = False  # are we in the info block?

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
                # special handling is required inside the info block
                if line.startswith("// ::: Transla"):  # !!!WARNING!!! THIS IS TIGHTLY COUPLED TO strings_txt_builder... if you change the header text there, it MUST MATCH HERE !!!!!!!!
                    info_block = True  # start the info block
                    f.write(f"{line}\n")

                elif info_block:
                    # if we are "inside the info block"
                    if line.startswith("///////"):
                        info_block = False  # leaving info block

                        # right before leaving the info block once and for all, we write out any additional stuff left in info_dict, and dump out comments

                        # dump info_dict
                        for k, v in info_dict.items():
                            f.write(f"//: {k}:")
                            f.write(f" {v}\n" if v else "\n")  # don't write a space unless there's a value

                        # dump out any comments... they should all be related to some info
                        #f.writelines(file_comments)
                        # no \r\n in the list, so do manually
                        for x in file_comments:
                            # skip special sequences

                            # headers
                            if x.startswith("// :::"):
                                continue
                            # auto-generated missing parts
                            if x.startswith("//! "):
                                continue
                            # help
                            if x.startswith("//? "):
                                continue
                            if x.startswith("//////////"):
                                continue

                            f.write(f"{x}\n")

                        f.write(f"{line}\n")   # write the actual end block line
                    else:
                        # we're inside the info block, check the keys against the info_dict
                        # !!!WARNING!!! THIS IS TIGHTLY COUPLED TO strings_txt_builder ... if the naming there changes, this will also be affected
                        (k, v) = line[4:].split(":", 1)
                        if k in info_dict:
                            # found same key info in the dict
                            f.write(f"//: {k}:")
                            f.write(f" {info_dict[k]}\n" if info_dict[k] else "\n")  # don't write a space unless there's a value

                            # delete it in info_dict
                            del info_dict[k]
                        else:
                            # not found in info_dict, write default value from reference template
                            f.write(f"//: {k}:")
                            f.write(f" {v}\n" if v else "\n")  # don't write a space unless there's a value

                else:
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

                continue

            # empty lines go over as well
            if len(line) == 0:
                f.write("\n")
                continue

            # what's remaining is the english string to split, there should be nothing on the other side though
            (e_index, e_none) = line.split("\t")
            if len(e_none) != 0:
                raise ValueError("Expected no value on reference")


            # increment count
            count_reference += 1



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
            f.write(f"\n// ::: INFO: Out-of-date Translations [{len(file_dict)}] ::: (Not in use anymore, safe to delete) //\n")
        for k, v in sorted(file_dict.items()):
            # make the dictionary sorted
            f.write(f"//{k}\t{v}\n")


        # dump out all missing translations to make it easier for a translator to know what needs to be done
        # we originally thought about making this a separate file, but that's actually more confusing
        if len(missing_list) != 0:
            f.write(f"\n// ::: INFO: Missing Translations [{len(missing_list)}] ::: (Summary of all missing strings to assist translators - Auto-generated by script) //\n")
        for k in missing_list:
            f.write(f"//! {k}\n")


        # write out a status line so we know when this was last checked by script
        f.write(f"\n// ::: STATUS: Last Checked/Updated by Script ::: {datetime.now(timezone.utc).isoformat()} //\n")
        # print more info regarding status of translation
        done_count = count_reference - len(missing_list)
        f.write(f"//! Translation Progress: {round(100 * done_count / count_reference)}%\n")
        f.write(f"//! * Total (Strings + Menu) = {count_reference}\n")
        f.write(f"//! * Done = {done_count}\n")
        f.write(f"//! * Missing = {len(missing_list)} ({round(100 * len(missing_list) / count_reference)}%)\n")


    return (missing_list, file_dict, count_reference)




def dump_strings_txt_summary_markdown():
    """
    Routine that prints out the status summary in GitHub markdown

    it's just as readable on the console, but this is designed for the wiki page
    """

    # dump out the progress in a neat summary

    print("| File        | Code | Language | Total | Missing | Done | % Done |")
    print("| ----------- | ---- | -------- | -----:| -------:| ----:| ------:|")

    running_total = 0
    running_done = 0

    for filepath in CONFIG_DIR.glob("strings_*.txt"):
        #code = filepath.stem[len("strings_"):]

        code = None
        lang = None

        percentage = None
        total = None
        done = None

        found_status_section = False
        with open(filepath, 'rt', encoding=UTF8) as f:
            while True:
                x = f.readline()
                if not x:
                    break

                if x.startswith("//: "):
                    # read from file, so we know we got the right stuff in the file
                    if "ISO 639" in x:
                        code = x[4:].split(":")[1].strip()
                    elif "Language" in x:
                        lang = x[4:].split(":")[1].strip()

                elif x.startswith("// ::: STATUS: Last"):
                    found_status_section = True
                elif found_status_section:
                    #print(x.strip())

                    # NOTE: this is tightly coupled with however I print in the method above
                    #  super hacky and just to get it done in the simplest way ... aka could return the counts from the previous method

                    if ":" in x:
                        percentage = x.split(":")[1].strip()
                    elif "Total" in x:
                        total = int(x.split("=")[1].strip())
                    elif "Done" in x:
                        done = int(x.split("=")[1].strip())

        #print(f"{desc:<6}:  {percentage:>4} => {total-done:4}")
        print(f"| [{filepath.name}](https://github.com/sylikc/jpegview/blob/master/src/JPEGView/Config/{filepath.name}) | {code} | {lang} | {total} | {total-done:4} | {done} | {percentage:>4} |")

        running_total += total
        running_done += done

    #print(f"| All |  |  | {running_total} | {running_total-running_done} | {running_done} | {round(100 * running_done / running_total)}% |")
    print(f"\nOverall Summary: Missing/Done/Total = {running_total-running_done} / {running_done} / {running_total} = {round(100 * running_done / running_total)}%\n")

    print(f"Last Updated: {datetime.now(timezone.utc).isoformat()}")




if __name__ == "__main__":
    # glob isn't a real list, but it's an iterable
    files_iterable = None

    # allow specifying languages on the command line
    if len(sys.argv) >= 2:
        files_iterable = []
        for x in sys.argv[1:]:
            #print(x)

            one_file = CONFIG_DIR / f"strings_{sys.argv[1]}.txt"

            if not one_file.exists():
                raise FileNotFoundError(one_file)

            files_iterable.append(one_file)
    else:
        files_iterable = CONFIG_DIR.glob("strings_*.txt")


    # only read the reference file once
    with open(REFERENCE_STRINGS, 'rt', encoding=UTF8) as f:
        ref_file = f.readlines()

    total = None
    for p in files_iterable:

        print(p)
        (missing, outdated, total) = sync_strings_to_reference(ref_file, p)
        #with open(p.with_suffix(".missing.txt"), 'wt', encoding=UTF8) as f:
        #    for x in missing:
        #        f.write(f"{x}\n")

    """
        reset files in git-bash

        $ find src/JPEGView/Config -name "strings_*.txt" -exec git checkout -- {} \;
    """

    print()
    dump_strings_txt_summary_markdown()



    #(missing, outdated) = sync_strings_to_reference(ref_file, CONFIG_DIR / "strings_fr_test.txt")
    #pprint.pprint(missing)
    #pprint.pprint(outdated)