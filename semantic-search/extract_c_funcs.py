import os, sys
import re
from glob import glob

if len(sys.argv) < 2:
    print("pass a root folder")
    sys.exit(1)
else:
    rootfolder = os.path.realpath(sys.argv[-1])

print("rootfolder:" + rootfolder)

c_files = [y for x in os.walk(rootfolder) for y in glob(os.path.join(x[0], '*.c'))]
cpp_files = [y for x in os.walk(rootfolder) for y in glob(os.path.join(x[0], '*.cpp'))]

filenames = c_files + cpp_files
# Define the regular expression pattern
ident = r'[a-zA-Z_][a-zA-Z0-9_]*'
retval = ident
#arg = r'\w+\s+\w+'
#arglist = r'\(([' + arg + r']*)\)'
arglist = r'\(([^)]*)\)'
pattern = retval + r'\s+(' + ident + r')\s*' + arglist + r'\s*\{([^}]+)\}'
count = 0

# Read the C++ code from a file
for fname in filenames:
    print("File: {}".format(fname))
    print("=========================")
    with open(fname, 'r') as f:
        code = f.read()
        # Search for the function using the pattern
        matches = re.findall(pattern, code)
        
        try:
            for match in matches:
                count += 1
                funcname = match[0]
                args = match[1]
                body = match[2]

                # Print the function arguments and body
                print(funcname)
                print(args)
                print(body)
                print("----------------------")
        except:
            pass

print("found {} files and {} functions".format(len(filenames), count))