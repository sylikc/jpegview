"""

It's a Util Library... things I use over and over again...

"""
import re
#from pathlib import Path


#################################################################################################

def get_all_text_between(filepath, pattern_begin, pattern_end, encoding=None, search_str=None):
    """ get all the text between the two patterns
    
    if filepath is None, assume we have the string in search_str
    """
    
    if filepath is None and search_str is None:
        raise ValueError
    
    if filepath is not None:
        if not filepath.exists():
            raise FileNotFoundError

        with open(filepath, 'rt', encoding=encoding) as f:
            search_str = f.read()
    
    m = re.search(f"{pattern_begin}(.*?){pattern_end}", search_str, re.MULTILINE | re.DOTALL)
    if m is None:
        raise LookupError(f"ERROR: couldn't find {pattern_begin}")

    # remove the len(pattern_end)
    return m.group(0)[:-len(pattern_end)]

#################################################################################################
