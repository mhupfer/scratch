import clang.cindex
from functools import reduce
import sys, os
from glob import glob
import re
import pickle
from openai.embeddings_utils import get_embedding
import argparse

MAX_LINES=10
LINES_THESHOLD =3
COMMENTS=r'\/\/.*|\/\*[\s\S]*?\*\/'

parser = argparse.ArgumentParser(description="")

parser.add_argument('-r', type=str, nargs=1, help='root folder', metavar='<root folder', default = [])
parser.add_argument('-q', type=str, nargs=1, help='query', metavar='<query>', default = [])
args = parser.parse_args()

rootfolder = args.r[0]
query = args.q[0]

print("rootfolder:" + rootfolder)

c_files = [y for x in os.walk(rootfolder) for y in glob(os.path.join(x[0], '*.c'))]
#cpp_files = [y for x in os.walk(rootfolder) for y in glob(os.path.join(x[0], '*.cpp'))]
cpp_files = []

filenames = c_files + cpp_files
'''
    Dictionary structure:
    filename->functions->function_name
                           ...
                         function_name->signature
                                        code_block->text
                                                    embedding
                                   ...
                                        signature
                                        code_block->text
                                                    embedding
            ->comments->line->text
                              embedding
                      ->line->text
                              embedding
'''

#################################################################
# get_source_code_of_statememt                                  #
#################################################################
def get_source_code_of_statememt(stmt):
    ss = stmt.extent.start.line # start of statement
    es = stmt.extent.end.line # end of statement
    # bug in lib requires additional line number check
    tokens = list(filter(lambda n:n.extent.start.line >= ss and n.extent.end.line <= es, stmt.get_tokens()))
    spellings = [n.spelling for n in tokens]
    body = reduce(lambda x,y:x+y, spellings, "")
    return body


#################################################################
# get_no_of_lines_of_statement                                  #
#################################################################
def get_no_of_lines_of_statement(stmt):
    return stmt.extent.end.line - stmt.extent.start.line + 1



#################################################################
# statement_must_be_splitted                                    #
#################################################################
def statement_must_be_splitted(stmt):
    lines = get_no_of_lines_of_statement(stmt)
    return lines > MAX_LINES + LINES_THESHOLD


#################################################################
# def create_code_block(stmt):                                  #
#################################################################
def create_code_block(body, line_of_code):
    ret = {'text': body, 'line':line_of_code, 'embedding': 0.0}
    return ret


#################################################################
# Split big statments into smaller ones                         #
#################################################################
def split_compound_statement(stmt, code_blocks):

    if not statement_must_be_splitted(stmt):
        body = get_source_code_of_statememt(stmt)
        code_blocks.append(create_code_block(body, stmt.extent.start.line))
    else:
        #split the code block into chunks of MAX_LINES
        #code_block = {'text': ""}
        tmp = []
        body = ""
        accumulated_lines = 0
        line_of_code = 0
        for child in stmt.get_children():
            if statement_must_be_splitted(child):
                if len(body) > 0:
                    tmp.append(create_code_block(body, line_of_code))
                
                body = ""
                accumulated_lines = 0

                split_compound_statement(child, tmp)
            else:
                if accumulated_lines == 0:
                    line_of_code = child.extent.start.line

                accumulated_lines += get_no_of_lines_of_statement(child)
                body += get_source_code_of_statememt(child)

                if accumulated_lines >= MAX_LINES - LINES_THESHOLD:
                    tmp.append(create_code_block(body, line_of_code))
                    body = ""
                    accumulated_lines = 0

        if len(body) > 0:
            tmp.append(create_code_block(body, line_of_code))

        #sometimes there are child although stmt is a
        #compount statement and should have
        #just append the complete statemet
        if len(tmp) == 0:
            line_of_code = stmt.extent.start.line
            body = get_source_code_of_statememt(stmt)
            tmp.append(create_code_block(body, line_of_code))

        code_blocks.extend(tmp)



#################################################################
# Traverse the AST to extract the function names and signatures #
#################################################################
def find_functions(tu, sources):
    for node in tu.cursor.walk_preorder():
        #if node.kind == clang.cindex.CursorKind.CXX_METHOD:
        #    filename = node.extent.start.file.name
        #    func_name = node.spelling
        #    print(func_name)
        # if node.kind.is_declaration():
        #     for comment in node.get_tokens().ast_nodes():
        #         if comment.kind.is_comment():
        #             print(get_source_code_of_statememt(comment))
        if node.extent.start.line == 5080:
            print("brich)")
        if node.kind == clang.cindex.CursorKind.FUNCTION_DECL or node.kind == clang.cindex.CursorKind.CXX_METHOD:
            filename = node.extent.start.file.name
            func_name = node.spelling

            if func_name == "dlt_set_log_mode" or func_name == "fwscanf":   
                print("Break")

            try:
                file_dict = sources[filename]['functions']
            except:
                sources[filename] = {'functions': {}}
                # sources[filename] = {}
                file_dict = sources[filename]['functions']

            line_of_code = node.extent.start.line
            try:
                func_dict = file_dict[(func_name, line_of_code)]
                continue
            except:
                func_dict = {"name": func_name, "signature":node.type.spelling, "code_blocks" : []}
                file_dict[(func_name, line_of_code)] = func_dict

            for c in node.get_children():
                #if threre's a COMPOUND_STMT it'S the function body
                #otherwise it's just a function declaration
                if c.kind == clang.cindex.CursorKind.COMPOUND_STMT:
                    code_blocks = []
                    split_compound_statement(c, code_blocks)
                    func_dict['code_blocks'] = code_blocks
                    break

            # print(f'Function: {func_name}({func_dict["signature"]}) \t{line_of_code}\t{filename}')
            # if len(func_dict['code_blocks']) > 0:
            #     print(func_dict['code_blocks'])
                
            # print("---------------------------")
        else:
            #print(f"node {node.spelling} kind {node.kind}")
            pass


#################################################################
# extract comments                                              #
#################################################################
def extract_comments(filename, sources):
    with open(fname, 'r') as f:
        code = f.read()
        # Search for the function using the pattern
        comments = re.findall(COMMENTS, code)
        line_no = 0
        
        try:
            for comment in comments:
                try:
                    sources[filename]['comments'][line_no] = {'text': comment, 'embedding': 0.0}
                except:
                    sources[filename]['comments'] = {line_no: {'text': comment, 'embedding': 0.0}}

                line_no += 1
                    
        except:
            pass

data_model_filename = os.path.join("./data", os.path.basename(rootfolder) + ".pickle")
try:
    with open(data_model_filename, "rb") as f:
        sources = pickle.load(f)
except:
    # Set up the Clang library
    clang.cindex.Config.set_library_file('/android/llvm-project/build/lib/libclang.so')
    index = clang.cindex.Index.create()
    sources = {}

    for fname in filenames:
        print("File: {}".format(fname))
        print("=========================")
        # Parse the C++ file and generate the AST
        tu = index.parse(fname)
        
        find_functions(tu, sources)
        extract_comments(fname, sources)

    print("Calculating embeddings ...")

    for f in sources.keys():
        for func in sources[f]['functions'].keys():
            print(func)

            for cb in sources[f]['functions'][func]['code_blocks']:
                cb['embedding'] = get_embedding(cb['text'], engine='text-embedding-ada-002')
                #cb['embedding'] = 8.8

        try:
            for l in sources[f]['comments'].keys():
                sources[f]['comments'][l]['embedding'] = get_embedding(sources[f]['comments'][l]['text'], engine='text-embedding-ada-002')
                #sources[f]['comments'][l]['embedding'] = 9.9
                #print("{}: {}".format(l, sources[f]['comments'][l]))
        except:
            pass


    with open(data_model_filename, "wb") as f:
        pickle.dump(sources, f, protocol=pickle.HIGHEST_PROTOCOL)

if len(query) > 0:
    print("Calculating similarities ...")
    from openai.embeddings_utils import cosine_similarity

    embedding = get_embedding(query, engine='text-embedding-ada-002')
    results = []

    for f in sources.keys():
        for func in sources[f]['functions'].keys():
            for cb in sources[f]['functions'][func]['code_blocks']:
                result = {'text': cb['text']}
                result['similarity'] = cosine_similarity(cb['embedding'], embedding)
                results.append(result)
                cb['embedding'] = get_embedding(cb['text'], engine='text-embedding-ada-002')
                #cb['embedding'] = 8.8

        try:
            for l in sources[f]['comments'].keys():
                pass
        except:
            pass

    results.sort(key=lambda x: x['similarity'])
    print(results)





