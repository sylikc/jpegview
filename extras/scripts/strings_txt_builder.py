"""

strings_XX.txt gets out of date very quickly, especially when coding... it's not easy to keep track of what changes were made which require localization

*** THIS CODE ISN'T MEANT TO BE PRETTY, IT IS MEANT TO JUST WORK ***
I'm absolutely sure this code is sloppy... lol



So, this is to auto-generate strings.txt


This was some basic research, before going all Python

findstr /l /s "CNLS::GetString" *.cpp *.h

exceptions:
GetTooltip in NavigationPanel.cpp

REM search for what you forgot to localize:
findstr /s /l "_T" *.cpp *.h | findstr /v /l "CNLS::GetString"

"""
import re
from pathlib import Path
from datetime import datetime, timezone
import pprint

from util_common import get_all_text_between


UTF8 = 'utf-8-sig'
# while it's not needed, all existing files have it, and I think it helps some editors, so leave it in
# https://stackoverflow.com/questions/2223882/whats-the-difference-between-utf-8-and-utf-8-with-bom


SOURCE_DIR = Path(__file__).resolve().parent.parent.parent / r"src\JPEGView"



#################################################################################################

def find_all_pattern(pattern, file_list=None, use_sets=False):
    """ find all strings with some pattern like _T(""), return dict

    file_list past in a list... [one file] works too

    use_sets uses sets to make sure what goes in the value of dict is a unique list
    """

    if file_list is None:
        # https://stackoverflow.com/questions/48181073/how-to-glob-two-patterns-with-pathlib/57893015#57893015
        exts = [".h", ".cpp"]
        files = [p for p in Path(SOURCE_DIR).rglob('*') if p.suffix in exts]
    else:
        files = file_list

    #print(list(files))

    list_all = {}

    for f in files:
        # search the file for patterns
        for s in re.findall(pattern, f.read_text()):
            # to figure out what file it came from
            if s in list_all:
                if use_sets:
                    list_all[s].add(f)
                else:
                    list_all[s].append(f)

            else:
                if use_sets:
                    list_all[s] = {f}
                else:
                    list_all[s] = [f]

    return list_all


#################################################################################################

def find_ini_usercommand():
    """ specific find function for JPEGView.ini for UserCommand... it's too much effort to try to massage find_all_pattern() into working for this specialized use case """

    # UserCmd0="KeyCode: Shift+Del Cmd: 'cmd /c del %filename%' Confirm: 'Do you really want to permanently delete the file %filename%?' HelpText: 'Shift+Del\tDelete current file on disk permanently' Flags: 'WaitForTerminate ReloadFileList MoveToNext'"
    #user_cmd = find_all_pattern(r'[^;]\s*UserCmd[0-9]+=".*?Confirm: \'(.*?)\'..........

    filepath = SOURCE_DIR / "Config/JPEGView.ini"

    ret = {}

    with open(filepath, 'rt', encoding=UTF8) as f:
        while True:
            line = f.readline()
            if not line:
                break

            # this type of matching isn't as reliable
            #if not line.startswith("UserCmd"):
            #    continue

            # find the pattern
            m = re.fullmatch(r'\s*UserCmd[0-9]+="(.*?)"\s*', line.strip())
            if m:
                cmd = m.group(1)
                print(f"Found UserCmd: {cmd}")

                m_confirm = re.search(r"Confirm:\s*'(.*?)'", cmd)
                if m_confirm:
                    confirm = m_confirm.group(1)
                    ret[confirm] = [filepath]  # forget about making duplicates

                m_helptext = re.search(r"HelpText:\s*'(.*?)'", cmd)
                if m_helptext:
                    helptext = m_helptext.group(1)
                    if r'\t' in helptext:  # if the sequence \t, not the actual tab
                        helptext = helptext.split(r'\t', 1)[1]

                    ret[helptext] = [filepath]

    return ret


#################################################################################################


def search_untranslated_strings(translated_t_dict):
    """
    look for _T() which are not translated, to see if they should be translated
    """

    # find all places where there is a string
    all_t = find_all_pattern(r'_T\s*\(\s*"(.*?)"\s*\)', use_sets=True)  # we don't need to know that it shows up multiple times in a file for this routine

    all_t.update(find_ini_usercommand())  # hack it in, for searchability

    # remove the known translated ones from all_t
    # to show what strings are NOT translated (at least in theory, since var_cnls_no_t might do it)
    for x in translated_t_dict.keys():
        del all_t[x]  # this should never throw an error as the patterns overlap


    # manual patterns to remove to make list more manageable
    temp_dict = all_t.copy()  # so we don't get a changing-while-reading error
    for k, v in temp_dict.items():
        # delete empty strings, or those which are empty after strip()
        if k.strip() == "":
            pass

        # extension strings, aka ".jpg"
        elif re.fullmatch("\.[a-z]{3}", k):
            pass

        # any strings of "*.ext" or "*.ext2"
        elif re.fullmatch("\*\.[a-z]{3,4}", k):
            pass

        # ignore strings of all symbols
        elif re.fullmatch("[\W]+", k):
            pass

        # ignore all numbers
        elif re.fullmatch("[\d\.]+", k):
            pass

        # begins and ends with <>
        elif k.startswith("<") and k.endswith(">"):
            pass

        # begins and ends with %
        elif k.startswith("%") and k.endswith("%"):
            pass

        # begins and ends with {}
        elif k.startswith("{") and k.endswith("}"):
            pass

        # ignore the ones that start with a / (command line params)
        elif k.startswith("/"):
            pass

        elif k.startswith("JPEGView"):
            pass

        # if string only exists in SettingsProvider
        elif len(v) == 1 and (SOURCE_DIR / "SettingsProvider.cpp") in v:
            pass

        # if string only exists in KeyMap
        elif len(v) == 1 and (SOURCE_DIR / "KeyMap.cpp") in v:
            pass

        # not enough characters, only one character in string
        elif not re.search("[a-zA-Z]{2}", k):
            pass

        else:
            # due to the logic above using pass, this has to skip the stuff below
            continue


        print(f"_T IGNORED: {k}")
        del all_t[k]




    # fixed things we are deleting, known not to need translation, list built by hand
    remove_from_all_t = [
        '%%%cx', '%%.%df', '%%0%cd', '%Nx : ', '%d KB', '%d MB', '%h %min : ', '%lf', '%min', '%pictures% : ',
        'rb',
    ]

    for x in remove_from_all_t:
        print(f"_T IGNORED: {k}")
        del all_t[x]

    return all_t


#################################################################################################


def parse_popup_menu_resource():
    """
    parse for strings in the resource's MENUs, as this doesn't match the _T() pattern
    """

    # this matches the popup menu pattern in source!
    menu_code = get_all_text_between(SOURCE_DIR / "JPEGView.rc", "POPUPMENU MENU", "//////////")

    #print(menu_code)

    # counter to ignore a certain count of lines after a certain trigger
    ignore_lines = 0

    # i don't use a set here because i want the menu items ordered the way they are listed... sets are unordered, and i don't want to sort the list
    ret_list = []

    for line in menu_code.split("\n"):
        line = line.strip()  # remove any leading or trailing whitespace

        if ignore_lines != 0:
            ignore_lines -= 1
            continue

        if re.fullmatch("[A-Z]+MENU MENU", line):  # this is a pattern that the code uses... ALL CAPS, then the word MENU, then the actual keyword MENU... after this, the first name POPUP "" does not need to be translated
            ignore_lines = 2
            continue

        # in any other case, seach for the general pattern, ignore all others
        # basically, just look for quotes
        if line.find('"') != -1:
            # NOTE: this MAY fail if you have "'s in the string, like \" or something... right now we don't have any, and we shouldn't have any in menus, but this code does NOT accommodate for that!
            m = re.search('"(.+)"', line)
            quote_str = m.group(1)  # get string in quotes

            # ignore certain patterns
            if quote_str == "_empty_":
                print(f"MENU IGNORED: {quote_str}")
                continue
            elif re.search("[a-zA-Z]{2}", quote_str) is None:
                # ignore lines where there's no contiguous characters to translate
                print(f"MENU IGNORED: {quote_str}")
                continue

            if "\\t" in quote_str:
                # special handling if there's a \t sequence as it means something different
                (first_part, second_part) = quote_str.split("\\t")  # it only splits into two

                # add the first part
                if first_part not in ret_list:
                    ret_list.append(first_part)

                if second_part.startswith("0x"):
                    print(f"MENU \\t IGNORED: {second_part}")
                    continue
                elif second_part == "Esc":
                    # special key sequence to ignore
                    print(f"MENU \\t IGNORED: {second_part}")
                    continue

                if second_part not in ret_list:
                    ret_list.append(second_part)

            else:
                if quote_str not in ret_list:
                    ret_list.append(quote_str)

    return ret_list


#################################################################################################



def dump_strings_txt(sorted_strings_dict, menu_list):
    """

    """
    menu_strings = menu_list.copy()  # because we're modifying it later

    OUT_FILE = SOURCE_DIR / "Config" / "strings.txt"

    # dump the dict out

    # UPDATE THE LASTUPDATED if this text gets updated so we know when translations are out of date!
    with open(OUT_FILE, 'wt', encoding=UTF8) as f:
        f.write(f"""//! STRINGS.TXT REFERENCE FILE: make a copy, remove this line, and follow the instructions to add/update languages -- Autogenerated, Last Updated: {datetime.now(timezone.utc).isoformat()}

//! This file must be encoded with UTF-8 (e.g. with Windows Notepad)
//! LICENSE: This localization file is part of JPEGView <https://github.com/sylikc/jpegview> and distributed under the same license.  See <https://github.com/sylikc/jpegview/blob/master/LICENSE.txt> for more info.

//! Standard  Filename Convention: 'strings_%languagecode%.txt' where %languagecode% is the ISO 639-1 language code e.g. 'en' for English (See <https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes>)
//! Alternate Filename Convention: 'strings_%languagecode%-%countrycode%.txt' where %countrycode% is the ISO 3166-1 country code e.g. 'US' for United States (See <https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes>)
//!    Use the alternate filename convention only when the standard filename already exist (e.g. specifying a country-specific dialect for an existing language)

////////////////////
// ::: Translation Info ::: //
//: ISO 639-1 code: en
//: ISO 3166-1 code:
//: Language: English
//: Translator(s) - Name <Contact Info/URL>:
//: Last Updated:
//: Additional Notes:
////////////////////

////////////////////
// ::: Instructions / Help ::: (help for translators only, does not need to be translated) //

//? Line Format:    English string<tab>Translated string<newline>
//? * Lines beginning with "//" (comments) are ignored
//? * Blank lines are ignored
//? * Lines without a <tab> are ignored
//? * Lines with no text after the <tab> are ignored
//? * Multiple tabs are permitted between English and Translated string, e.g. this is valid:    English string<tab><tab><tab>Translated string<newline>

//? NOTE: %s %d have special meanings in the strings, so make sure they appear somewhere in the translated string!

//? For the strings below which are short and have no context:
//?   "in"        in context: is shorthand for "inch"
//?   "cm"        in context: is shorthand for "centimeter"
//?   "C - R"     in context: is "Cyan Red" colors
//?   "M - G"     in context: is "Magenta Green" colors
//?   "Y - B"     in context: is "Yellow Blue" colors
//?   "yes"/"no"  in context: is for showing EXIF flags
//?   "on"/"off"  in context: is for showing a setting active or not
//?   "Filter"    in context: is used in Resize image, "sampling filter"
//?   "Box"       in context: is used in Resize image sampling filter "Box filter"
////////////////////


// ::: Program Strings ::: (sorted alphabetically) //
""")
        keys = list(sorted_strings_dict.keys())
        #keys.sort(key=str.casefold)  # case-insensitive
        keys.sort()  # leave it case sensitive, as the lower case is easy to reference for "in" and "cm"
        for k in keys:
            #f.write(f"{k}\t{k}\n")  # sample just has string<tab>string
            f.write(f"{k}\t\n")  # sample with string<tab>string is hard to translate and fill in

            # do you want to know what file(s) the string came from?  for debugging only
            #f.write(f"{k}\t{d[k]}\n")

            # remove this entry if it exists in the menu strings (so there's no duplicate translation strings)
            try:
                menu_strings.remove(k)
                print(f"Duplicate string from menu removed: {k}")
            except ValueError:
                pass

        f.write("""
// ::: Popup Menu Strings ::: (unsorted to preserve context) //
""")
        # these aren't sorted alphabetically, for clarity (and context)
        for k in menu_strings:
            #f.write(f"{k}\t{k}\n")  # sample just has string<tab>string
            f.write(f"{k}\t\n")  # string<tab>string too hard to find diffs or untranslated text



if __name__ == "__main__":

    all_cnls_t = find_all_pattern(r'CNLS::GetString\s*\(\s*_T\s*\(\s*"(.*?)"\s*\)\s*\)')

    # exception in this particular function call
    nav_tooltip_t = find_all_pattern(r'GetTooltip\([\w\->]+,\s*_T\s*\(\s*"(.*?)"\s*\)', [SOURCE_DIR / "NavigationPanel.cpp"])
    #pprint.pprint(nav_tooltip_t)
    all_cnls_t.update(nav_tooltip_t)

    # user commands get translated when read.  so whatever is default, translate it.  custom-added, users are on their own
    user_cmd = find_ini_usercommand()
    #pprint.pprint(user_cmd)
    all_cnls_t.update(user_cmd)

    # these are also special, in that it's translated by HelpersGUI.cpp =>  CNLS::GetString(menuText)
    popup_list = parse_popup_menu_resource()

    #pprint.pprint(popup_list)


    #print(all_cnls_t)
    dump_strings_txt(all_cnls_t, popup_list)


    all_t = search_untranslated_strings(all_cnls_t)

    print(f"{'!' * 30} NOT TRANSLATED {'!' * 30}")
    pprint.pprint(all_t)
    # ----------------------------------------------------------------------------------------



    # find places where GetString is used without _T, meaning it's of a variable
    print()
    print(f"{'~' * 30} GetString() without _T() {'~' * 30}")
    var_cnls_no_t = find_all_pattern(r'CNLS::GetString\s*\(\s*([^_].*?)\s*\)', use_sets=True)
    pprint.pprint(var_cnls_no_t)
    # one of the places is in the INI there is Help Text= '' for usercommand... this is fed into translation... confirmation is as well
