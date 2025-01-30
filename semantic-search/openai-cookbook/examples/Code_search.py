#!/usr/bin/env python
# coding: utf-8

# ## Code search
# 
# We index our own [openai-python code repository](https://github.com/openai/openai-python), and show how it can be searched. We implement a simple version of file parsing and extracting of functions from python files.


import os, sys
from glob import glob
import pandas as pd
#import argparse
from openai.embeddings_utils import get_embedding

def get_function_name(code):
    """
    Extract function name from a line beginning with "def "
    """
    assert code.startswith("def ")
    return code[len("def "): code.index("(")]

def get_until_no_space(all_lines, i) -> str:
    """
    Get all lines until a line outside the function definition is found.
    """
    ret = [all_lines[i]]
    for j in range(i + 1, i + 10000):
        if j < len(all_lines):
            if len(all_lines[j]) == 0 or all_lines[j][0] in [" ", "\t", ")"]:
                ret.append(all_lines[j])
            else:
                break
    return "\n".join(ret)

def get_functions(filepath):
    """
    Get all functions in a Python file.
    """
    whole_code = open(filepath).read().replace("\r", "\n")
    all_lines = whole_code.split("\n")
    for i, l in enumerate(all_lines):
        if l.startswith("def "):
            code = get_until_no_space(all_lines, i)
            function_name = get_function_name(code)
            yield {"code": code, "function_name": function_name, "filepath": filepath}



if len(sys.argv) < 2:
    print("pass a query")
    sys.exit(1)
else:
    query = sys.argv[-1]

try:
    df = pd.read_pickle("data/code_search_openai-python.pickle")
except:
    # get user root directory
    root_dir = os.path.expanduser("~")
    # note: for this code to work, the openai-python repo must be downloaded and placed in your root directory

    # path to code repository directory
    code_root = root_dir + "/scratch/semantic-search/openai-python"

    code_files = [y for x in os.walk(code_root) for y in glob(os.path.join(x[0], '*.py'))]
    print("Total number of py files:", len(code_files))

    if len(code_files) == 0:
        print("Double check that you have downloaded the openai-python repo and set the code_root variable correctly.")

    all_funcs = []
    for code_file in code_files:
        funcs = list(get_functions(code_file))
        for func in funcs:
            all_funcs.append(func)

    print("Total number of functions extracted:", len(all_funcs))

    df = pd.DataFrame(all_funcs)
    df['code_embedding'] = df['code'].apply(lambda x: get_embedding(x, engine='text-embedding-ada-002'))
    df['filepath'] = df['filepath'].apply(lambda x: x.replace(code_root, ""))
    df.to_pickle("data/code_search_openai-python.pickle")


from openai.embeddings_utils import cosine_similarity

def search_functions(df, code_query, n=3, pprint=True, n_lines=7):
    embedding = get_embedding(code_query, engine='text-embedding-ada-002')
    df['similarities'] = df.code_embedding.apply(lambda x: cosine_similarity(x, embedding))

    res = df.sort_values('similarities', ascending=False).head(n)
    if pprint:
        for r in res.iterrows():
            print(r[1].filepath+":"+r[1].function_name + "  score=" + str(round(r[1].similarities, 3)))
            print("\n".join(r[1].code.split("\n")[:n_lines]))
            print('-'*70)
    return res

res = search_functions(df, query, n=3)


# # In[4]:


# res = search_functions(df, 'fine-tuning input data validation logic', n=3)


# # In[5]:


# res = search_functions(df, 'find common suffix', n=2, n_lines=10)


# # In[6]:


# res = search_functions(df, 'Command line interface for fine-tuning', n=1, n_lines=20)

