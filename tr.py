#!/usr/bin/python3
import deepl
import sys
from functools import reduce

##########################
## print_usage          ##
##########################
def print_usage():
    print("tr [-s source language] [-d dest language] -r ...")
    print("-s\t source language, default DE")
    print("-d\t destination language, default EN-US")
    print("-r\t roundtrip, src->dest->src")
    sys.exit(0);

def deepl_src_lang(lang):
    if lang.startswith("EN"):
        return "EN"
    else:
        return lang

def deepl_dest_lang(lang):
    if lang == "EN":
        return "EN-US"
    else:
        return lang

##########################
## main                 ##
##########################

if len(sys.argv) == 1:
    print_usage()
    sys.exit(0)

mode = "plain"
idx = 1
dest_lang="EN-US";
src_lang="DE"

while idx < len(sys.argv):
    if sys.argv[idx] == "-r":
        mode = "roundtrip"
        idx += 1;
        continue
    if sys.argv[idx] == "-s":
        if idx + 1 < len(sys.argv):
            src_lang = sys.argv[idx+1]
            idx += 2
            continue
        else:
            print_usage()
    if sys.argv[idx] == "-d":
        if idx + 1 < len(sys.argv):
            dest_lang = sys.argv[idx+1]
            idx += 2
            continue
        else:
            print_usage()

    break

if dest_lang == "EN":
    dest_lang = "EN-US"

#create a string from the argument array
src = reduce((lambda x, y: x + " " + y), sys.argv[idx::])

if src == "":
    print_usage()

#translate
auth_key = "534b61b6-164e-4491-ab7c-2c3ad37808e9:fx"
translator = deepl.Translator(auth_key)

result = translator.translate_text(src, target_lang=deepl_dest_lang(dest_lang), \
                                   source_lang = deepl_src_lang(src_lang))

if mode == "plain":
    print(result.text)
elif mode == "roundtrip":
    result2 = translator.translate_text(result.text, target_lang=deepl_dest_lang(src_lang), \
                                        source_lang = deepl_src_lang(dest_lang))
    print(result2.text)
else:
    print("Fatal we should not end up here: mode = " + mode)


